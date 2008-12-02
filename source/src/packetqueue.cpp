// supports buffering Enet packets for logging purposes

#include "pch.h"
#include "cube.h"

packetqueue::packetqueue(size_t size)
{
    queuesize = size;
}

packetqueue::~packetqueue()
{
    clear();
}

// adds packet to log buffer
void packetqueue::queue(ENetPacket &p)
{
    // add to back
    pqueue.push(p);
    // drop oldest packet to fit size
    if(pqueue.size() > queuesize) pqueue.pop();
}

// writes all currently queued packets to disk and clears the queue
bool packetqueue::flushtolog(char *logfile )
{
    if(pqueue.empty()) return false;

    FILE *f = NULL;
    if(logfile && logfile[0]) f = openfile(logfile, "w");
    if(!f) return false;
    
    // header
    fprintf(f, "AC PACKET LOG %11d\n\n", now_utc);
    // serialize each packet
    while(!pqueue.empty())
    {
        ENetPacket packet = pqueue.front();
        
        fputs("ENET PACKET\n", f);
        fprintf(f, "flags == %d\n", packet.flags);
        fprintf(f, "referenceCount == %d\n", packet.referenceCount);
        fprintf(f, "dataLength == %d\n", packet.dataLength);
        fputs("data == \n", f);
        // print whole buffer char-wise
        loopi(packet.dataLength)
        {
            fprintf(f, "%16d\n", packet.data[i]);
        }

        pqueue.pop();
    }

    fclose(f);
    clear();
    return true;
}

// clear queue
void packetqueue::clear()
{
    while(!pqueue.empty()) pqueue.pop();
}

