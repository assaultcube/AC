// supports buffering Enet packets for logging purposes

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

    stream *f = NULL;
    if(logfile && logfile[0]) f = openfile(logfile, "w");
    if(!f) return false;

    // header
    f->printf("AC v%d PACKET LOG : proto %d : @ %11s\n\n", AC_VERSION, PROTOCOL_VERSION, numtime());
    // serialize each packet
    loopv(packets)
    {
        ENetPacket *p = packets[i];

        f->printf("ENET PACKET\n");
        f->printf("flags == %d\n", p->flags);
        f->printf("referenceCount == %d\n", (int)p->referenceCount);
        f->printf("dataLength == %d\n", (int)p->dataLength);
        f->printf("data == \n");
        // print whole buffer char-wise
        loopj(p->dataLength)
        {
            f->printf("%16d\n", p->data[j]);
        }
    }

    delete f;
    clear();
    return true;
}

// clear queue
void packetqueue::clear()
{
    loopv(packets) enet_packet_destroy(packets[i]);
    packets.clear();
}

