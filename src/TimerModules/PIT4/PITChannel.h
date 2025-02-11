#pragma once

#include "Arduino.h"
#include "ITimerChannel.h"

namespace TeensyTimerTool
{
    class PIT_t;

    class PITChannel : public ITimerChannel
    {
     public:
        inline PITChannel(unsigned nr);
        inline virtual ~PITChannel();

        inline errorCode begin(callback_t cb, float tcnt, bool periodic) override;
        //inline errorCode begin(callback_t cb, uint32_t tcnt, bool periodic) override;
        inline errorCode start() override;
        inline errorCode startIRQ() override;
        inline errorCode stop() override;
        inline errorCode stopIRQ() override;

        //inline errorCode trigger(uint32_t) override;
        inline errorCode trigger(float) override;
        inline errorCode setNextPeriod(float us) override;
        inline errorCode setPeriod(float us) override;

        inline float getMaxPeriod() const override;

        bool isPeriodic;

     protected:
        //IMXRT_GPT_t* regs;
        //uint32_t reload;

        inline void isr();

        PITChannel()                   = delete;
        PITChannel(const PITChannel &) = delete;

        const unsigned chNr;
        callback_t callback = nullptr;

        static uint32_t clockFactor;

        friend PIT_t;
    };

    // IMPLEMENTATION ==============================================

    PITChannel::PITChannel(unsigned nr)
        : ITimerChannel(nullptr), chNr(nr)
    {
        callback    = nullptr;

        // IDK why this does not work...
        //clockFactor = (CCM_CSCMR1 & CCM_CSCMR1_PERCLK_CLK_SEL) ? 24 : (F_BUS_ACTUAL / 1000000);

        clockFactor = USE_GPT_PIT_150MHz ? 150 : 60;
    }

    errorCode PITChannel::begin(callback_t cb, float micros, bool periodic)
    {
        isPeriodic = periodic;
        callback   = cb;

        if (isPeriodic)
        {
            IMXRT_PIT_CHANNELS[chNr].TCTRL = 0;
            IMXRT_PIT_CHANNELS[chNr].TFLG  = 1;

            setNextPeriod(micros);
            // float tmp = micros * clockFactor;
            // if (tmp > 0xFFFF'FFFF)
            // {
            //     postError(errorCode::periodOverflow);
            //     IMXRT_PIT_CHANNELS[chNr].LDVAL = 0xFFFF'FFFE;
            // } else
            // {
            //     IMXRT_PIT_CHANNELS[chNr].LDVAL = (uint32_t)tmp - 1;
            // }
        }
        return errorCode::OK;
    }

    errorCode PITChannel::start()
    {
        IMXRT_PIT_CHANNELS[chNr].TCTRL = PIT_TCTRL_TEN | PIT_TCTRL_TIE;
        return errorCode::OK;
    }

    errorCode PITChannel::startIRQ()
    {
        //NVIC_DISABLE_IRQ(IRQ_PIT);
        NVIC_CLEAR_PENDING(IRQ_PIT);
        NVIC_ENABLE_IRQ(IRQ_PIT);
        IMXRT_PIT_CHANNELS[chNr].TCTRL = PIT_TCTRL_TEN | PIT_TCTRL_TIE;
        return errorCode::OK;
    }

    errorCode PITChannel::stop()
    {
        IMXRT_PIT_CHANNELS[chNr].TCTRL = 0;
        return errorCode::OK;
    }

    errorCode PITChannel::stopIRQ()
    {
        NVIC_DISABLE_IRQ(IRQ_PIT);

        IMXRT_PIT_CHANNELS[chNr].TCTRL = 0;

        // for (unsigned i = 0; i < 4; i++)
        // {
        //     IMXRT_PIT_CHANNELS[i].TCTRL = 0;
        // }

        NVIC_CLEAR_PENDING(IRQ_PIT);
        NVIC_ENABLE_IRQ(IRQ_PIT);


        return errorCode::OK;
    }

    errorCode PITChannel::setNextPeriod(float us)
    {
        float cts = us * clockFactor;
        if (cts > 0xFFFF'FFFF)
        {
            postError(errorCode::periodOverflow);
            IMXRT_PIT_CHANNELS[chNr].LDVAL = 0xFFFF'FFFE;
        } else if ((uint32_t)cts < 1)
        {
            postError(errorCode::periodOverflow);
            IMXRT_PIT_CHANNELS[chNr].LDVAL = 0; // Should be legal (and equal to 1 cycle)
        }
        else
        {
            IMXRT_PIT_CHANNELS[chNr].LDVAL = (uint32_t)cts - 1;
        }
        return errorCode::OK;
    }

    errorCode PITChannel::setPeriod(float us)
    {
        bool wasRunning = IMXRT_PIT_CHANNELS[chNr].TCTRL & PIT_TCTRL_TEN;

        stop(); // need to stop/start timer to change current period (see ch 53.9.5.4)
        
        setNextPeriod(us);

        if (wasRunning) start();
        
        return errorCode::OK;
    }

    void PITChannel::isr()
    {
        if (callback != nullptr)
        {
            callback();
            if (!isPeriodic) IMXRT_PIT_CHANNELS[chNr].TCTRL = 0; // switch off timer
        }
    }

    PITChannel::~PITChannel()
    {
        callback = nullptr;
    }

    errorCode PITChannel::trigger(float delay) //should be optimized somehow
    {
        IMXRT_PIT_CHANNELS[chNr].TCTRL = 0;
        IMXRT_PIT_CHANNELS[chNr].TFLG  = 1;

        float tmp = delay * clockFactor;
        if (tmp > 0xFFFF'FFFF)
        {
            postError(errorCode::periodOverflow);
            IMXRT_PIT_CHANNELS[chNr].LDVAL = 0xFFFF'FFFE;
        } else if ((uint32_t)tmp < 1)
        {
            postError(errorCode::periodOverflow);
            IMXRT_PIT_CHANNELS[chNr].LDVAL = 0; // Should be legal (and equal to 1 cycle)
        }
        else
        {
            IMXRT_PIT_CHANNELS[chNr].LDVAL = (uint32_t)tmp - 1;
        }
        start();

        return errorCode::OK;
    }

    float PITChannel::getMaxPeriod() const
    {
        return (float)0xFFFF'FFFE / clockFactor / 1'000'000;
    }

} // namespace TeensyTimerTool