// very simple http get function

#include "cube.h"
#include <errno.h>

#define CR 13
#define LF 10
#define TCP_TIMEOUT 5678      // only re-use quite fresh connections
#define GETERROR(msg) { err = msg; goto geterror; }

const char lfcrlf[] = { LF, CR, LF, 0x00 }, *crlf = lfcrlf + 1;

void httpget::reset(int keep) // keep == 0: delete all, keep == 1: keep connection, keep == 2: keep connection and url
{
    if(keep < 1)
    { // reset connection
        DELSTRING(hostname);
        ip.host = ENET_HOST_ANY;
        ip.port = 80;         // change manually, if needed
        maxredirects = 3;
        maxtransfer = maxsize = 1<<20; // 1 MB (caps transfer size and unzipped size)
        connecttimeout = 6000;
        disconnect();
    }
    err = NULL;
    response = chunked = gzipped = contentlength = offset = elapsedtime = traffic = 0;
    if(keep < 2) DELSTRING(url);
    DELSTRING(header);
    rawsnd.setsize(0);
    rawrec.setsize(0);
    datarec.setsize(0);
}

bool httpget::set_host(const char *newhostname) // set and resolve host
{
    if(!hostname || strcmp(hostname, newhostname))
    {
        DELSTRING(hostname);
        disconnect(); // new or different host name -> close connection (if still open)

        ENetAddress newhost;
        if(enet_address_set_host(&newhost, newhostname) < 0)
        {
            err = "failed to resolve hostname";
            return false;
        }
        ip.host = newhost.host;
        hostname = newstring(newhostname);
    }
    return ip.host != ENET_HOST_ANY;
}

void httpget::set_port(int port) // set port (disconnect, if different to previous)
{
    port &= 65535;
    if(port < 80) port = 80;
    if(port != ip.port) disconnect();
    ip.port = port;
}

bool httpget::execcallback(float progress)
{
    if(callbackfunc && (*callbackfunc)(callbackdata, progress))
    {
        err = "manual abort";
        return true;
    }
    return false;
}

void httpget::disconnect()
{
    if(tcp != ENET_SOCKET_NULL)
    {
        enet_socket_destroy(tcp);
        tcp = ENET_SOCKET_NULL;
    }
}

struct connectinfo { ENetSocket sock; ENetAddress address; volatile char running; };

int hcont(void *data)
{
    connectinfo ci = *((connectinfo *)data);
    ((connectinfo *)data)->running = 1; // now the calling thread may as well vanish
    return enet_socket_connect(ci.sock, &ci.address);
}

bool httpget::connect(bool force)
{
    if(force || tcp_age.elapsed() > TCP_TIMEOUT) disconnect();
    if(tcp == ENET_SOCKET_NULL)
    {
        connectinfo ci = { enet_socket_create(ENET_SOCKET_TYPE_STREAM), ip, 0 };
        tcp_age.start();
        if(ci.sock == ENET_SOCKET_NULL)
        {
            err = "could not open socket";
            return false;
        }
        void *ti = sl_createthread(hcont, &ci);
        loopk(connecttimeout / 250 + 1)
        {
            sl_detachthread(NULL); // cleanup call
            sl_sleep(250);
            if(ci.running) // wait for the connect thread to copy its data
            {
                if(sl_pollthread(ti))
                {
                    if(sl_waitthread(ti) < 0)
                    {
                        err = "could not connect";
                        ti = NULL;
                        break;
                    }
                    else
                    {
                        tcp = ci.sock;
                        return true;
                    }
                }
                if(execcallback(-float(tcp_age.elapsed()) / (connecttimeout| 1))) break; // esc pressed
            }
        }
        sl_detachthread(ti);
        if(!err) err = "could not connect in time";
        enet_socket_destroy(ci.sock);
        ASSERT(ci.running);
        return false;
    }
    return true;
}

int httpget::get(const char *url1, uint timeout, uint totaltimeout, int range, bool head)
{
    int redirects = 0, res = 0, transferred = 0;
    vector<char> *content = NULL;
    ASSERT(timeout && totaltimeout); // zero is not allowed
    stopwatch to;
    to.start();
    uint lastresponse = 0;
    reset(1); // clear old response values and url
    url = newstring(url1);

    bool reconnected = false, retry = false;
    for(;;)
    { // (in a loop because of possible redirects)
        if(!connect()) goto geterror;

        // assemble GET request
        cvecprintf(rawsnd, "%s %s HTTP/1.1%s", head ? "HEAD" : "GET", url, crlf);
        cvecprintf(rawsnd, "Host: %s%s", hostname, crlf);
        cvecprintf(rawsnd, "Accept: */*%s", crlf);
        if(referrer) cvecprintf(rawsnd, "Referer: %s%s", referrer, crlf);
        if(useragent) cvecprintf(rawsnd, "User-Agent: %s%s", useragent, crlf);
        if(range > 0) cvecprintf(rawsnd, "Range: bytes=%d-%sTE: gzip%s", range, crlf, crlf);  // content-encoding gzip for range requests is nonsense, so we'll ask for transfer-encoding instead (not that any server will deliver that...)
        else if(!head) cvecprintf(rawsnd, "Accept-Encoding: gzip%s", crlf); // ask for gzip only for full requests, because we intend to unzip instantly
        cvecprintf(rawsnd, "%s", crlf);

        // send request
        execcallback(0);
        ENetBuffer buf;
        buf.data = rawsnd.getbuf();
        buf.dataLength = rawsnd.length();
        while(buf.dataLength > 0)
        {
            enet_uint32 events = ENET_SOCKET_WAIT_SEND;
            if(enet_socket_wait(tcp, &events, 250) >= 0 && events)
            {
                int sent = enet_socket_send(tcp, NULL, &buf, 1);
                if(sent < 0)
                {
                    if(!reconnected)
                    { // line went dead, maybe the server closed it - try reconnecting once
                        retry = true;
                        break;
                    }
                    else GETERROR("connection failed");
                }
                buf.data = (char *)buf.data + sent;
                buf.dataLength -= sent;
                tcp_age.start();
                traffic += sent;
                lastresponse = to.elapsed();
            }
            elapsedtime = to.elapsed();
            if(elapsedtime > totaltimeout || elapsedtime > lastresponse + timeout) GETERROR("timeout 1");
            if(execcallback(-float(elapsedtime - lastresponse) / timeout)) goto geterror; // show progress bar, check for user interrupt
        }

        // get response
        bool again = false, haveheader = false;
        if(!retry) for(;;)
        {
            elapsedtime = to.elapsed();
            if(elapsedtime > totaltimeout || elapsedtime > lastresponse + timeout) GETERROR(elapsedtime > totaltimeout ? "timeout 2" : "timeout 3");
            float progress = -float(elapsedtime - lastresponse) / timeout;
            enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
            if(rawrec.length() > maxsize || datarec.length() > maxsize) break;
            if(enet_socket_wait(tcp, &events, 250) >= 0 && events)
            {
                if(rawrec.length() >= rawrec.capacity()) rawrec.reserve(4096);
                buf.data = rawrec.getbuf() + rawrec.length();
                buf.dataLength = rawrec.capacity() - rawrec.length();
                int recv = enet_socket_receive(tcp, NULL, &buf, 1);
                if(recv <= 0)
                {
                    if(!haveheader && !reconnected) retry = true; // line may be half dead - retry once
                    break;
                }
                rawrec.advance(recv);
                tcp_age.start();
                traffic += recv;
                if((transferred += recv) > maxtransfer) GETERROR("transfer size exceeds limit");
                lastresponse = to.elapsed();

                // parsing received data
                rawrec.add('\0'); // append '\0' to use string functions safely - will be removed again
                if(!haveheader)
                { // still getting header
                    size_t headendlen = strlen(lfcrlf);
                    const char *endhead = strstr((const char *)rawrec.getbuf(), lfcrlf);
                    if(!endhead && --headendlen) endhead = strstr((const char *)rawrec.getbuf(), "\n\n"); // we're tolerant to LF only line endings
                    if(endhead)
                    {
                        size_t headlen = endhead - rawrec.getbuf() + headendlen;
                        header = newstring(rawrec.getbuf(), headlen);
                        rawrec.remove(0, headlen);
                        haveheader = true;

                        // parse header
                        char *header_uc = filtertext(newstring(headlen), header, FTXT_TOUPPER, headlen);
                        char *p = strstr(header, "HTTP/");
                        if(p) p = strchr(p, ' ');
                        response = p ? atoi(p + 1) : -1;
                        if(p) p = strchr(p, '\n');
                        if(!p) GETERROR("server error"); // header frame error
                        if(response >= 300 && response < 308) // handle redirects
                        {
                            p = strstr(header_uc + (p - header), "LOCATION: ");
                            if(p) p = header + (p - header_uc);
                            DELSTRING(header_uc);
                            if(++redirects > maxredirects || !p) GETERROR(p ? "too many redirects" : "redirect error");
                            // read redirect url
                            disconnect();
                            p = strchr(p, ' ') + 1;
                            char *r = strstr(p, "://");
                            if(r) p = r + 3;
                            if((r = strchr(p, '/')))
                            {
                                *r = '\0';
                                again = set_host(p);
                                *r = '/';
                                p = r + strcspn(r, " \n\r");
                                *p = '\0';
                                const char *nurl = newstring(r);
                                reset(1);
                                url = nurl;
                            }
                            if(!again) GETERROR("bad redirect url");
                            break; // done receiving
                        }
                        // response was 2xx, check for some extras
                        char *u = header_uc + (p - header), *f, *n;
                        if((f = strstr(u, "TRANSFER-ENCODING: ")) && (n = strchr(f, LF)))
                        {
                            if((f = strstr(f, "CHUNKED")) && f < n) chunked = 1;
                            if((f = strstr(f, "GZIP")) && f < n) gzipped = 1; // untested: I could not find a server who delivers that
                        }
                        if((f = strstr(u, "CONTENT-ENCODING: ")) && (n = strchr(f, LF)) && (f = strstr(f, "GZIP")) && f < n) gzipped = 1;
                        if((f = strstr(u, "CONTENT-LENGTH: ")) && (f = strchr(f, ' '))) contentlength = atoi(f + 1);
                        if((f = strstr(u, "CONTENT-RANGE: ")) && (n = strchr(f, LF)) && (f = strstr(f, "BYTES ")) && (f = strchr(f, ' ')) && f < n) offset = atoi(f + 1);
                        DELSTRING(header_uc);
                    }
                }
                if(haveheader)
                { // check if data is complete
                    const char *d = rawrec.getbuf(), *e = d;
                    if(chunked)
                    { // n * "chunksize\r\nchunkdata\r\n" + "0\r\n\r\n"
                        while(strchr(d, LF))
                        {
                            int chunklen = (int) strtol(d, (char**) &e, 16);
                            if(*e == CR) e++;
                            if(*e == LF && rawrec.length() >= chunklen + (e - d) + 2 && (e[chunklen + 1] == LF || (e[chunklen + 1] == CR && e[chunklen + 2] == LF)))
                            { // chunk is fully in buffer
                                e++;
                                loopi(chunklen) datarec.add(*e++);
                                if(*e == CR) e++;
                                if(*e++ != LF) GETERROR("chunk frame error");
                                rawrec.remove(0, int(e - d));
                                if(chunklen == 0)
                                {
                                    contentlength = datarec.length();
                                    content = &datarec;
                                }
                            }
                            else break;
                        }
                        progress = float(datarec.length()) / (maxtransfer | 1); // not accurate, but moving
                    }
                    else
                    {
                        if(contentlength > 0)
                        {
                            progress = float(rawrec.length() - 1) / contentlength; // get a real progress bar
                            if(rawrec.length() - 1 >= contentlength) content = &rawrec;
                        }
                        else progress = float(rawrec.length() - 1) / (maxtransfer | 1); // not accurate, but moving
                    }
                }
                rawrec.drop();
            }
            if(execcallback(progress)) goto geterror; // show progress bar, check for user interrupt
            if(content || (haveheader && head)) break;
        }
        if(retry && !reconnected)
        { // TCP connection showed some error before the response header was fully received: retry the connection once, it may have just been a timeout
            again = reconnected = true;
            reset(2); // only clear response values, not url
            disconnect(); // but close the socket
            retry = false;
        }
        if(again) continue; // redirect
        else break;
    }
    if(!content) content = &rawrec; // end by "server close connection" - this should maybe be flagged as error instead
    if(gzipped)
    {
        z_stream zs;
        memset((void *)&zs, 0, sizeof(zs));
        if(inflateInit2(&zs, 15 + 32) == Z_OK)
        {
            zs.next_in = (Bytef*)content->getbuf();
            zs.avail_in = content->length();
            uchar outbuf[1<<15];
            int zstat;
            do
            {
                zs.next_out = (Bytef*)outbuf;
                zs.avail_out = sizeof(outbuf);
                zstat = inflate(&zs, Z_NO_FLUSH);
                if(zstat == Z_OK || zstat == Z_STREAM_END)
                {
                    int got = sizeof(outbuf) - zs.avail_out;
                    if(got > 0)
                    {
                        res += got;
                        if(outvec) loopi(got) outvec->add(outbuf[i]);
                        if(outstream) outstream->write(outbuf, got);
                    }
                }
                else GETERROR("gunzip error");
                if(res > maxsize) break;
            }
            while(zstat == Z_OK);
            inflateEnd(&zs);
        }
    }
    else
    {
        res = content->length();
        if(outvec) outvec->insert(outvec->length(), (const uchar *)content->getbuf(), res);
        if(outstream) outstream->write(content->getbuf(), res);
    }

geterror:
    if(res > maxsize) err = "output truncated";
    elapsedtime = to.elapsed();
    if(err) disconnect();
    return err ? -1 : res;
}

char *urlencode(const char *s, bool strict)
{
    vector<char> d;
    while(*s)
    {
        if(isalnum(*s) || strchr("-_.~", *s) || (!strict && strchr(":/?#[]@!$&'()*+,;=", *s))) d.add(*s++); // see RFC 3986 2.2
        else cvecprintf(d, "%%%02X", *s++);
    }
    d.add('\0');
    return newstring(d.getbuf());
}

// parse URLs: scheme:[//[user:password@]domain[:port]][/]path[?query][#fragment]

void urlparse::set(const char *newurl)
{
    DELSTRING(buf);
    char *p, *s = buf = newstring(newurl, strlen(newurl) + 3);
    scheme = userpassword = port = path = query = fragment = "";

    filtertext(s, s, FTXT_NOWHITE);
    if((p = strstr(s, "://"))) // domain part is mandatory, which makes the double slash a given - if a scheme is specified
    { // scheme found
        scheme = s;
        *p = '\0';
        s = p + 3;
    }
    if((p = strchr(s, '@')))
    { // user:password found
        userpassword = s;
        *p = '\0';
        s = p + 1;
    }
    domain = s;
    if(*s == '[' && (p = strchr(s, ']'))) s = p + 1; // [....]
    else while(isalnum(*s) || *s == '.' || *s == '-') s++; // skip domain
    if(*s == ':')
    { // port found
        *s = '\0';
        port = ++s;
        while(isdigit(*s)) s++;
    }
    if(*s)
    {
        memmove(s + 2, s, strlen(s) + 1);
        *s++ = '\0'; // end domain or port string
        *s++ = '/'; // add mandatory slash for path
        if(*s != '/') s--;
        path = s;
        if((p = strchr(s, '?')))
        { // query found
            *p++ = '\0';
            query = s = p;
        }
        if((p = strchr(s, '#')))
        { // fragment found
            *p++ = '\0';
            fragment = s = p;
        }
        // fix path, if necessary
        size_t pl = strlen(path);
        if(pl < 2) path = ""; // ignore single slash
        else
        {
            p = (char *)path;
            unixpath(p);
            p += pl - 1;
            if(*p == '/') *p = '\0'; // remove trailing slash
        }
    }
}
