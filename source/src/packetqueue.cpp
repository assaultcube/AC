// supports buffering Enet packets for logging purposes

#include "pch.h"
#include "cube.h"

packetqueue::packetqueue()
{
}

packetqueue::~packetqueue()
{
    clear();
}

// adds packet to log buffer
void packetqueue::queue(ENetPacket *p)
{
    if(packets.length() >= packets.maxsize()) enet_packet_destroy(packets.remove());
    packets.add(p);
}

// writes all currently queued packets to disk and clears the queue
bool packetqueue::flushtolog(const char *logfile)
{
    if(packets.empty()) return false;

    FILE *f = NULL;
    if(logfile && logfile[0]) f = openfile(logfile, "w");
    if(!f) return false;

    // header
    fprintf(f, "AC PACKET LOG %11s\n\n", numtime());
    // serialize each packet
    loopv(packets)
    {
        ENetPacket *p = packets[i];

        fputs("ENET PACKET\n", f);
        fprintf(f, "flags == %d\n", p->flags);
        fprintf(f, "referenceCount == %d\n", (int)p->referenceCount);
        fprintf(f, "dataLength == %d\n", (int)p->dataLength);
        fputs("data == \n", f);
        // print whole buffer char-wise
        loopj(p->dataLength)
        {
            fprintf(f, "%16d\n", p->data[j]);
        }
    }

    fclose(f);
    clear();
    return true;
}

// clear queue
void packetqueue::clear()
{
    loopv(packets) enet_packet_destroy(packets[i]);
    packets.clear();
}

