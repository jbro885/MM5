/*
 * Copyright (c) 2002, 2003, 2004, 2005
 * The Regents of The University of Michigan
 * All Rights Reserved
 *
 * This code is part of the M5 simulator, developed by Nathan Binkert,
 * Erik Hallnor, Steve Raasch, and Steve Reinhardt, with contributions
 * from Ron Dreslinski, Dave Greene, Lisa Hsu, Kevin Lim, Ali Saidi,
 * and Andrew Schultz.
 *
 * Permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any
 * purpose, so long as the copyright notice above, this grant of
 * permission, and the disclaimer below appear in all copies made; and
 * so long as the name of The University of Michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION FROM THE
 * UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY PURPOSE, AND
 * WITHOUT WARRANTY BY THE UNIVERSITY OF MICHIGAN OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE
 * LIABLE FOR ANY DAMAGES, INCLUDING DIRECT, SPECIAL, INDIRECT,
 * INCIDENTAL, OR CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGES.
 */

/* @file
 * Device module for modelling a fixed bandwidth full duplex ethernet link
 */

#include <cmath>
#include <deque>
#include <string>
#include <vector>

#include "base/trace.hh"
#include "dev/etherdump.hh"
#include "dev/etherint.hh"
#include "dev/etherlink.hh"
#include "dev/etherpkt.hh"
#include "sim/builder.hh"
#include "sim/serialize.hh"
#include "sim/system.hh"
#include "sim/root.hh"

using namespace std;

EtherLink::EtherLink(const string &name, EtherInt *peer0, EtherInt *peer1,
		     double rate, Tick delay, EtherDump *dump)
    : SimObject(name)
{
    link[0] = new Link(name + ".link0", this, 0, rate, delay, dump);
    link[1] = new Link(name + ".link1", this, 1, rate, delay, dump);

    interface[0] = new Interface(name + ".int0", link[0], link[1]);
    interface[1] = new Interface(name + ".int1", link[1], link[0]);

    interface[0]->setPeer(peer0);
    peer0->setPeer(interface[0]);
    interface[1]->setPeer(peer1);
    peer1->setPeer(interface[1]);
}

EtherLink::~EtherLink()
{
    delete link[0];
    delete link[1];

    delete interface[0];
    delete interface[1];
}

EtherLink::Interface::Interface(const string &name, Link *tx, Link *rx)
    : EtherInt(name), txlink(tx)
{
    tx->setTxInt(this);
    rx->setRxInt(this);
}

EtherLink::Link::Link(const string &name, EtherLink *p, int num,
                      double rate, Tick delay, EtherDump *d)
    : objName(name), parent(p), number(num), txint(NULL), rxint(NULL),
      ticksPerByte(rate), linkDelay(delay), dump(d), 
      doneEvent(this)
{ }

void
EtherLink::serialize(ostream &os)
{
    link[0]->serialize("link0", os);
    link[1]->serialize("link1", os);
}

void
EtherLink::unserialize(Checkpoint *cp, const string &section)
{
    link[0]->unserialize("link0", cp, section);
    link[1]->unserialize("link1", cp, section);
}

void
EtherLink::Link::txComplete(PacketPtr packet)
{
    DPRINTF(Ethernet, "packet received: len=%d\n", packet->length);
    DDUMP(EthernetData, packet->data, packet->length);
    rxint->sendPacket(packet);
}

class LinkDelayEvent : public Event
{
  protected:
    EtherLink::Link *link;
    PacketPtr packet;

  public:
    // non-scheduling version for createForUnserialize()
    LinkDelayEvent();
    LinkDelayEvent(EtherLink::Link *link, PacketPtr pkt, Tick when);

    void process();

    virtual void serialize(ostream &os);
    virtual void unserialize(Checkpoint *cp, const string &section);
    static Serializable *createForUnserialize(Checkpoint *cp,
                                              const string &section);
};

void
EtherLink::Link::txDone()
{
    if (dump)
        dump->dump(packet);

    if (linkDelay > 0) {
        DPRINTF(Ethernet, "packet delayed: delay=%d\n", linkDelay);
        new LinkDelayEvent(this, packet, curTick + linkDelay);
    } else {
        txComplete(packet);
    }

    packet = 0;
    assert(!busy());

    txint->sendDone();
}

bool
EtherLink::Link::transmit(PacketPtr pkt)
{
    if (busy()) {
        DPRINTF(Ethernet, "packet not sent, link busy\n");
        return false;
    }

    DPRINTF(Ethernet, "packet sent: len=%d\n", pkt->length);
    DDUMP(EthernetData, pkt->data, pkt->length);

    packet = pkt;
    Tick delay = (Tick)ceil(((double)pkt->length * ticksPerByte) + 1.0);
    DPRINTF(Ethernet, "scheduling packet: delay=%d, (rate=%f)\n",
	    delay, ticksPerByte);
    doneEvent.schedule(curTick + delay);

    return true;
}

void
EtherLink::Link::serialize(const string &base, ostream &os)
{
    bool packet_exists = packet;
    paramOut(os, base + ".packet_exists", packet_exists);
    if (packet_exists)
	packet->serialize(base + ".packet", os);

    bool event_scheduled = doneEvent.scheduled();
    paramOut(os, base + ".event_scheduled", event_scheduled);
    if (event_scheduled) {
	Tick event_time = doneEvent.when();
        paramOut(os, base + ".event_time", event_time);
    }

}

void
EtherLink::Link::unserialize(const string &base, Checkpoint *cp, 
                             const string &section)
{
    bool packet_exists;
    paramIn(cp, section, base + ".packet_exists", packet_exists);
    if (packet_exists) {
	packet = new PacketData(16384);
	packet->unserialize(base + ".packet", cp, section);	
    }
    
    bool event_scheduled;
    paramIn(cp, section, base + ".event_scheduled", event_scheduled);
    if (event_scheduled) {
	Tick event_time;
        paramIn(cp, section, base + ".event_time", event_time);
	doneEvent.schedule(event_time);
    }
}

LinkDelayEvent::LinkDelayEvent()
    : Event(&mainEventQueue), link(NULL)
{
    setFlags(AutoSerialize);
    setFlags(AutoDelete);
}

LinkDelayEvent::LinkDelayEvent(EtherLink::Link *l, PacketPtr p, Tick when)
    : Event(&mainEventQueue), link(l), packet(p)
{
    setFlags(AutoSerialize);
    setFlags(AutoDelete);
    schedule(when);
}

void
LinkDelayEvent::process()
{
    link->txComplete(packet);
}

void
LinkDelayEvent::serialize(ostream &os)
{
    paramOut(os, "type", string("LinkDelayEvent"));
    Event::serialize(os);

    EtherLink *parent = link->parent;
    bool number = link->number;
    SERIALIZE_OBJPTR(parent);
    SERIALIZE_SCALAR(number);

    packet->serialize("packet", os);
}


void
LinkDelayEvent::unserialize(Checkpoint *cp, const string &section)
{
    Event::unserialize(cp, section);

    EtherLink *parent;
    bool number;
    UNSERIALIZE_OBJPTR(parent);
    UNSERIALIZE_SCALAR(number);

    link = parent->link[number];

    packet = new PacketData(16384);
    packet->unserialize("packet", cp, section);
}


Serializable *
LinkDelayEvent::createForUnserialize(Checkpoint *cp, const string &section)
{
    return new LinkDelayEvent();
}

REGISTER_SERIALIZEABLE("LinkDelayEvent", LinkDelayEvent)

BEGIN_DECLARE_SIM_OBJECT_PARAMS(EtherLink)

    SimObjectParam<EtherInt *> int1;
    SimObjectParam<EtherInt *> int2;
    Param<double> speed;
    Param<Tick> delay;
    SimObjectParam<EtherDump *> dump;

END_DECLARE_SIM_OBJECT_PARAMS(EtherLink)

BEGIN_INIT_SIM_OBJECT_PARAMS(EtherLink)

    INIT_PARAM(int1, "interface 1"),
    INIT_PARAM(int2, "interface 2"),
    INIT_PARAM(speed, "link speed in bits per second"),
    INIT_PARAM(delay, "transmit delay of packets in us"),
    INIT_PARAM(dump, "object to dump network packets to")

END_INIT_SIM_OBJECT_PARAMS(EtherLink)

CREATE_SIM_OBJECT(EtherLink)
{
    return new EtherLink(getInstanceName(), int1, int2, speed, delay, dump);
}

REGISTER_SIM_OBJECT("EtherLink", EtherLink)
