/*
 * Copyright (c) 2004, 2005
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

/** @file
 * Tsunami I/O Space mapping including RTC/timer interrupts
 */

#ifndef __DEV_TSUNAMI_IO_HH__
#define __DEV_TSUNAMI_IO_HH__

#include "dev/io_device.hh"
#include "base/range.hh"
#include "dev/tsunami.hh"
#include "sim/eventq.hh"

class MemoryController;

/**
 * Tsunami I/O device is a catch all for all the south bridge stuff we care
 * to implement.
 */
class TsunamiIO : public PioDevice
{
  private:
    /** The base address of this device */
    Addr addr;
    
    /** The size of mappad from the above address */
    static const Addr size = 0xff;

    struct tm tm;

  protected:
    /** Real-Time Clock (MC146818) */
    class RTC
    {
      private:
        /** Event for RTC periodic interrupt */
        struct RTCEvent : public Event
        {
            /** A pointer back to tsunami to create interrupt the processor. */
            Tsunami* tsunami;
            Tick interval;

            RTCEvent(Tsunami* t, Tick i);

            /** Schedule the RTC periodic interrupt */
            void scheduleIntr();

            /** Event process to occur at interrupt*/
            virtual void process();

            /** Event description */
            virtual const char *description();
        };

      private:
        std::string _name;
        const std::string &name() const { return _name; }

        /** RTC periodic interrupt event */
        RTCEvent event;

        /** Current RTC register address/index */
        int addr;

        /** Data for real-time clock function */
        union {
            uint8_t clock_data[10];

            struct {
                uint8_t sec;
                uint8_t sec_alrm;
                uint8_t min;
                uint8_t min_alrm;
                uint8_t hour;
                uint8_t hour_alrm;
                uint8_t wday;
                uint8_t mday;
                uint8_t mon;
                uint8_t year;
            };
        };

        /** RTC status register A */
        uint8_t stat_regA;

        /** RTC status register B */
        uint8_t stat_regB;

      public:
        RTC(const std::string &name, Tsunami* t, Tick i);

        /** Set the initial RTC time/date */
        void set_time(time_t t);

        /** RTC address port: write address of RTC RAM data to access */
        void writeAddr(const uint8_t *data);

        /** RTC write data */
        void writeData(const uint8_t *data);

        /** RTC read data */
        void readData(uint8_t *data);

        /**
          * Serialize this object to the given output stream.
          * @param os The stream to serialize to.
          */
        void serialize(const std::string &base, std::ostream &os);

        /**
         * Reconstruct the state of this object from a checkpoint.
         * @param cp The checkpoint use.
         * @param section The section name of this object
         */
        void unserialize(const std::string &base, Checkpoint *cp,
                         const std::string &section);
    };

    /** Programmable Interval Timer (Intel 8254) */
    class PITimer
    {
        /** Counter element for PIT */
        class Counter
        {
            /** Event for counter interrupt */
            class CounterEvent : public Event
            {
              private:
                /** Pointer back to Counter */
                Counter* counter;
                Tick interval;

              public:
                CounterEvent(Counter*);

                /** Event process */
                virtual void process();

                /** Event description */
                virtual const char *description();

                friend class Counter;
            };

          private:
            std::string _name;
            const std::string &name() const { return _name; }

            CounterEvent event;

            /** Current count value */
            uint16_t count;

            /** Latched count */
            uint16_t latched_count;

            /** Interrupt period */
            uint16_t period;

            /** Current mode of operation */
            uint8_t mode;

            /** Output goes high when the counter reaches zero */
            bool output_high;

            /** State of the count latch */
            bool latch_on;

            /** Set of values for read_byte and write_byte */
            enum {LSB, MSB};

            /** Determine which byte of a 16-bit count value to read/write */
            uint8_t read_byte, write_byte;

          public:
            Counter(const std::string &name);

            /** Latch the current count (if one is not already latched) */
            void latchCount();

            /** Set the read/write mode */
            void setRW(int rw_val);

            /** Set operational mode */
            void setMode(int mode_val);

            /** Set count encoding */
            void setBCD(int bcd_val);

            /** Read a count byte */
            void read(uint8_t *data);

            /** Write a count byte */
            void write(const uint8_t *data);

            /** Is the output high? */
            bool outputHigh();

            /**
             * Serialize this object to the given output stream.
             * @param os The stream to serialize to.
             */
            void serialize(const std::string &base, std::ostream &os);

            /**
             * Reconstruct the state of this object from a checkpoint.
             * @param cp The checkpoint use.
             * @param section The section name of this object
             */
            void unserialize(const std::string &base, Checkpoint *cp,
                             const std::string &section);
        };

      private:
        std::string _name;
        const std::string &name() const { return _name; }

        /** PIT has three seperate counters */
        Counter *counter[3];

      public:
        /** Public way to access individual counters (avoid array accesses) */
        Counter counter0;
        Counter counter1;
        Counter counter2;

        PITimer(const std::string &name);

        /** Write control word */
        void writeControl(const uint8_t* data);

        /**
         * Serialize this object to the given output stream.
         * @param os The stream to serialize to.
         */
        void serialize(const std::string &base, std::ostream &os);

        /**
         * Reconstruct the state of this object from a checkpoint.
         * @param cp The checkpoint use.
         * @param section The section name of this object
         */
        void unserialize(const std::string &base, Checkpoint *cp,
                         const std::string &section);
    };

    /** Mask of the PIC1 */
    uint8_t mask1;
    
    /** Mask of the PIC2 */
    uint8_t mask2;
    
    /** Mode of PIC1. Not used for anything */
    uint8_t mode1;

    /** Mode of PIC2. Not used for anything */
    uint8_t mode2;

    /** Raw PIC interrupt register before masking */
    uint8_t picr; //Raw PIC interrput register

    /** Is the pic interrupting right now or not. */
    bool picInterrupting;

    Tick clockInterval;

    /** A pointer to the Tsunami device which be belong to */
    Tsunami *tsunami;

    /** Intel 8253 Periodic Interval Timer */
    PITimer pitimer;

    RTC rtc;

    /** The interval is set via two writes to the PIT.
     * This variable contains a flag as to how many writes have happened, and
     * the time so far.
     */
    uint16_t timerData;

  public:
    /** 
     * Return the freqency of the RTC 
     * @return interrupt rate of the RTC
     */
    Tick frequency() const;

    /**
     * Initialize all the data for devices supported by Tsunami I/O. 
     * @param name name of this device.
     * @param t pointer back to the Tsunami object that we belong to.
     * @param init_time Time (as in seconds since 1970) to set RTC to.
     * @param a address we are mapped at.
     * @param mmu pointer to the memory controller that sends us events.
     */
    TsunamiIO(const std::string &name, Tsunami *t, time_t init_time,
              Addr a, MemoryController *mmu, HierParams *hier, Bus *bus,
	      Tick pio_latency, Tick ci);

    /**
      * Process a read to one of the devices we are emulating.
      * @param req Contains the address to read from.
      * @param data A pointer to write the read data to.
      * @return The fault condition of the access.
      */
    virtual Fault read(MemReqPtr &req, uint8_t *data);

    /**
      * Process a write to one of the devices we emulate. 
      * @param req Contains the address to write to.
      * @param data The data to write.
      * @return The fault condition of the access.
      */
    virtual Fault write(MemReqPtr &req, const uint8_t *data);

    /**
     * Post an PIC interrupt to the CPU via the CChip
     * @param bitvector interrupt to post.
     */
    void postPIC(uint8_t bitvector);

    /** 
     * Clear a posted interrupt
     * @param bitvector interrupt to clear
     */
    void clearPIC(uint8_t bitvector);

    /**
     * Serialize this object to the given output stream.
     * @param os The stream to serialize to.
     */
    virtual void serialize(std::ostream &os);

    /**
     * Reconstruct the state of this object from a checkpoint.
     * @param cp The checkpoint use.
     * @param section The section name of this object
     */
    virtual void unserialize(Checkpoint *cp, const std::string &section);
          
    Tick cacheAccess(MemReqPtr &req);
};

#endif // __DEV_TSUNAMI_IO_HH__
