/**
 *   @file  dss_data_path.c
 *
 *   @brief
 *      Implements Data path processing functionality.
 *
 *  \par
 *  NOTE:
 *      (C) Copyright 2016 Texas Instruments, Inc.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**************************************************************************
 *************************** Include Files ********************************
 **************************************************************************/

/* Standard Include Files. */
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* BIOS/XDC Include Files. */
#include <xdc/std.h>
#include <xdc/cfg/global.h>
#include <xdc/runtime/IHeap.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Event.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/heaps/HeapBuf.h>
#include <ti/sysbios/heaps/HeapMem.h>
#include <ti/sysbios/knl/Event.h>
#if defined (SUBSYS_DSS)
#include <ti/sysbios/family/c64p/Hwi.h>
#include <ti/sysbios/family/c64p/EventCombiner.h>
#endif
#define DebugP_ASSERT_ENABLED 1
#include <ti/drivers/osal/DebugP.h>
#include <assert.h>
#include <ti/common/sys_common.h>
#include <ti/drivers/osal/SemaphoreP.h>
#include <ti/drivers/edma/edma.h>
#include <ti/drivers/esm/esm.h>
#include <ti/drivers/soc/soc.h>
#include <ti/utils/cycleprofiler/cycle_profiler.h>

#include <ti/alg/mmwavelib/mmwavelib.h>

/* C64P dsplib (fixed point part for C674X) */
#include "gen_twiddle_fft32x32.h"
#include "gen_twiddle_fft16x16.h"
#include "DSP_fft32x32.h"
#include "DSP_fft16x16.h"

/* C674x mathlib */
#include <ti/mathlib/mathlib.h>

#include "dss_data_path.h"
#include "dss_config_edma_util.h"

/* If the the following EDMA defines are commented out, the EDMA transfer completion is
   is implemented using polling apporach, Otherwise, if these defines are defined, the EDMA transfers 
   are implemented using blocking approach, (data path task pending on semaphore, waiting for the transfer completion 
   event, posted by EDMA transfer completion intedrupt. In these cases since the EDMA transfers are faster than 
   DSP processing, polling approach is more appropriate. */
//#define EDMA_1D_INPUT_BLOCKING
//#define EDMA_1D_OUTPUT_BLOCKING
//#define EDMA_2D_INPUT_BLOCKING
//#define EDMA_2D_OUTPUT_BLOCKING
//#define EDMA_MATRIX2_INPUT_BLOCKING
//#define EDMA_3D_INPUT_BLOCKING

#define MMW_ADCBUF_SIZE     0x4000U

/*! @brief L2 heap used for allocating buffers in L2 SRAM,
    mostly scratch buffers */
#define MMW_L2_HEAP_SIZE    0xC000U

/*! @brief L1 heap used for allocating buffers in L1D SRAM,
    mostly scratch buffers */
#define MMW_L1_HEAP_SIZE    0x4000U

/*! L3 RAM buffer */
#pragma DATA_SECTION(gMmwL3, ".l3data");
#pragma DATA_ALIGN(gMmwL3, 8);
uint8_t gMmwL3[SOC_XWR16XX_DSS_L3RAM_SIZE];

/*! L2 Heap */
#pragma DATA_SECTION(gMmwL2, ".l2data");
#pragma DATA_ALIGN(gMmwL2, 8);
uint8_t gMmwL2[MMW_L2_HEAP_SIZE];

/*! L1 Heap */
#pragma DATA_SECTION(gMmwL1, ".l1data");
#pragma DATA_ALIGN(gMmwL1, 8);
uint8_t gMmwL1[MMW_L1_HEAP_SIZE];

/*! Types of FFT window */
#define FFT_WINDOW_INT16 0
#define FFT_WINDOW_INT32 1

/* FFT Window */
/*! Hanning window */
#define MMW_WIN_HANNING  0
/*! Blackman window */
#define MMW_WIN_BLACKMAN 1
/*! Rectangular window */
#define MMW_WIN_RECT     2


void MmwDemo_genWindow(void *win,
                        uint32_t windowDatumType,
                        uint32_t winLen,
                        uint32_t winGenLen,
                        int32_t oneQformat,
                        uint32_t winType);
                        

#define MMW_EDMA_CH_1D_IN_PING      EDMA_TPCC0_REQ_FREE_0
#define MMW_EDMA_CH_1D_IN_PONG      EDMA_TPCC0_REQ_FREE_1
#define MMW_EDMA_CH_1D_OUT_PING     EDMA_TPCC0_REQ_FREE_2
#define MMW_EDMA_CH_1D_OUT_PONG     EDMA_TPCC0_REQ_FREE_3
#define MMW_EDMA_CH_2D_IN_PING      EDMA_TPCC0_REQ_FREE_4
#define MMW_EDMA_CH_2D_IN_PONG      EDMA_TPCC0_REQ_FREE_5
#define MMW_EDMA_CH_DET_MATRIX      EDMA_TPCC0_REQ_FREE_6
#define MMW_EDMA_CH_DET_MATRIX2     EDMA_TPCC0_REQ_FREE_7
#define MMW_EDMA_CH_3D_IN_PING      EDMA_TPCC0_REQ_FREE_8
#define MMW_EDMA_CH_3D_IN_PONG      EDMA_TPCC0_REQ_FREE_9

#define MMW_EDMA_TRIGGER_ENABLE  1
#define MMW_EDMA_TRIGGER_DISABLE 0

#define ROUND(_x) (_x < 0 ? (_x - 0.5) : (_x + 0.5) )


extern volatile cycleLog_t gCycleLog;

/**
 *  @b Description
 *  @n
 *      Resets the Doppler line bit mask. Doppler line bit mask indicates Doppler
 *      lines (bins) on wich the CFAR in Doppler direction detected objects.
 *      After the CFAR in Doppler direction is completed for all range bins, the
 *      CFAR in range direction is performed on indicated Doppler lines.
 *      The array dopplerLineMask is uint32_t array. The LSB bit of the first word
 *      corresponds to Doppler line (bin) zero.
 *
 */
void MmwDemo_resetDopplerLines(MmwDemo_1D_DopplerLines_t * ths)
{
    memset((void *) ths->dopplerLineMask, 0 , ths->dopplerLineMaskLen * sizeof(uint32_t));
    ths->currentIndex = 0;
}

/**
 *  @b Description
 *  @n
 *      Sets the bit in the Doppler line bit mask dopplerLineMask corresponding to Doppler
 *      line on which CFAR in Doppler direction detected object. Indicating the Doppler
 *      line being active in observed frame. @sa MmwDemo_resetDopplerLines
 */
void MmwDemo_setDopplerLine(MmwDemo_1D_DopplerLines_t * ths, uint16_t dopplerIndex)
{
    uint32_t word = dopplerIndex >> 5;
    uint32_t bit = dopplerIndex & 31;

    ths->dopplerLineMask[word] |= (0x1 << bit);
}

/**
 *  @b Description
 *  @n
 *      Checks whetehr Doppler line is active in the observed frame. It checks whether the bit
 *      is set in the Doppler line bit mask corresponding to Doppler
 *      line on which CFAR in Doppler direction detected object.
 *      @sa MmwDemo_resetDopplerLines
 */
uint32_t MmwDemo_isSetDopplerLine(MmwDemo_1D_DopplerLines_t * ths, uint16_t index)
{
    uint32_t dopplerLineStat;
    uint32_t word = index >> 5;
    uint32_t bit = index & 31;

    if(ths->dopplerLineMask[word] & (0x1 << bit))
    {
        dopplerLineStat = 1;
    }
    else
    {
        dopplerLineStat = 0;
    }
    return dopplerLineStat;
}

/**
 *  @b Description
 *  @n
 *      Gets the Doppler index from the Doppler line bit mask, starting from the
 *      smallest active Doppler lin (bin). Subsequent calls return the next
 *      active Doppler line. @sa MmwDemo_resetDopplerLines
 *
 */
int32_t MmwDemo_getDopplerLine(MmwDemo_1D_DopplerLines_t * ths)
{
    uint32_t index = ths->currentIndex;
    uint32_t word = index >> 5;
    uint32_t bit = index & 31;

    while (((ths->dopplerLineMask[word] >> bit) & 0x1) == 0)
    {
        index++;
        bit++;
        if (bit == 32)
        {
            word++;
            bit = 0;
            if (word >= ths->dopplerLineMaskLen)
            {
                DebugP_assert(0);
            }
        }
    }
    ths->currentIndex = index + 1;
    return index;
}


/**
 *  @b Description
 *  @n
 *      Power of 2 round up function.
 */
uint32_t MmwDemo_pow2roundup (uint32_t x)
{
    uint32_t result = 1;
    while(x > result)
    {
        result <<= 1;
    }
    return result;
}

/**
 *  @b Description
 *  @n
 *      Calculates X/Y coordinates in meters based on the maximum position in
 *      the magnitude square of the azimuth FFT output. The function is called
 *      per detected object. The detected object structure already has populated
 *      range and doppler indices. This function finds maximum index in the
 *      azimuth FFT, calculates X and Y and coordinates and stores them into
 *      object fields along with the peak height. Also it populates the azimuth
 *      index in azimuthMagSqr array.
 *
 *  @param[in] obj                Pointer to data path object
 *
 *  @param[in] objIndex           Detected objet index
 *
 *  @retval
 *      NONE
 */
void MmwDemo_XYestimation(MmwDemo_DSS_DataPathObj *obj,
                          uint32_t objIndex)
{
    uint32_t i;
    int32_t sMaxIdx;
    float temp;
    float Wx;
    float range;
    float x, y;
    float *azimuthMagSqr = obj->azimuthMagSqr;
    float rangeResolution = obj->rangeResolution;
    uint32_t xyzOutputQFormat = obj->xyzOutputQFormat;
#define ONE_QFORMAT (1 << xyzOutputQFormat)
    uint32_t numAngleBins = obj->numAngleBins;
    uint32_t numDopplerBins = obj->numDopplerBins;
    uint32_t numRangeBins = obj->numRangeBins;

    uint16_t azimIdx = 0;
    float maxVal = 0;

    /* Find peak position */
    for (i = 0; i < numAngleBins; i++)
    {
        if (azimuthMagSqr[i] > maxVal)
        {
            azimIdx = i;
            maxVal = azimuthMagSqr[i];
        }
    }
    /* Save azimuth index, for debug purpose */
    obj->detObj2dAzimIdx[objIndex] = azimIdx;

    /* Save square root of maximum peak to object peak value scaling */
    temp = divsp(maxVal, (numRangeBins * numAngleBins * numDopplerBins));
    obj->detObj2D[objIndex].peakVal = (uint16_t)sqrtsp(temp);

    /* Calculate X and Y coordiantes in meters in Q8 format */
    range = obj->detObj2D[objIndex].rangeIdx * rangeResolution;

    if(azimIdx > (numAngleBins/2 -1))
    {
        sMaxIdx = azimIdx - numAngleBins;
    }
    else
    {
        sMaxIdx = azimIdx;
    }

    Wx = 2 * (float) sMaxIdx / numAngleBins;
    x = range * Wx;

    /* y = sqrt(range^2 - x^2) */
    temp = range*range -x*x ;
    if (temp > 0)
    {
        y = sqrtsp(temp);
    }
    else
    {
        y = 0;
    }

    /* Convert to Q8 format */
    obj->detObj2D[objIndex].x = (int16_t) ROUND(x * ONE_QFORMAT);
    obj->detObj2D[objIndex].y = (int16_t) ROUND(y * ONE_QFORMAT);
    obj->detObj2D[objIndex].z = 0;

    /* Check for second peak */
    if (obj->multiObjBeamFormingCfg.enabled)
    {
        uint32_t leftSearchIdx;
        uint32_t rightSearchIdx;
        uint32_t secondSearchLen;
        uint32_t iModAzimLen;
        float maxVal2;
        int32_t k;

        /* Find right edge of the first peak */
        i = azimIdx;
        leftSearchIdx = (i + 1) & (numAngleBins-1);
        k = numAngleBins;
        while ((azimuthMagSqr[i] >= azimuthMagSqr[leftSearchIdx]) && (k > 0))
        {
            i = (i + 1) & (numAngleBins-1);
            leftSearchIdx = (leftSearchIdx + 1) & (numAngleBins-1);
            k--;
        }

        /* Find left edge of the first peak */
        i = azimIdx;
        rightSearchIdx = (i - 1) & (numAngleBins-1);
        k = numAngleBins;
        while ((azimuthMagSqr[i] >= azimuthMagSqr[rightSearchIdx]) && (k > 0))
        {
            i = (i - 1) & (numAngleBins-1);
            rightSearchIdx = (rightSearchIdx - 1) & (numAngleBins-1);
            k--;
        }

        secondSearchLen = ((rightSearchIdx - leftSearchIdx) & (numAngleBins-1)) + 1;
        /* Find second peak */
        maxVal2 = azimuthMagSqr[leftSearchIdx];
        azimIdx = leftSearchIdx;
        for (i = leftSearchIdx; i < (leftSearchIdx + secondSearchLen); i++)
        {
            iModAzimLen = i & (numAngleBins-1);
            if (azimuthMagSqr[iModAzimLen] > maxVal2)
            {
                azimIdx = iModAzimLen;
                maxVal2 = azimuthMagSqr[iModAzimLen];
            }
        }

        /* Is second peak greater than threshold? */
        if (maxVal2 > (maxVal * obj->multiObjBeamFormingCfg.multiPeakThrsScal) && (obj->numDetObj < MMW_MAX_OBJ_OUT))
        {
            /* Second peak detected! Add it to the end of the list */
            obj->detObj2D[obj->numDetObj].dopplerIdx = obj->detObj2D[objIndex].dopplerIdx;
            obj->detObj2D[obj->numDetObj].rangeIdx = obj->detObj2D[objIndex].rangeIdx;
            objIndex = obj->numDetObj;
            obj->numDetObj++;
            maxVal = maxVal2;

            /* Save azimuth index, for debug purpose */
            obj->detObj2dAzimIdx[objIndex] = azimIdx;

            /* Save square root of maximum peak to object peak value scaling */
            temp = divsp(maxVal, (numRangeBins * numAngleBins * numDopplerBins));
            obj->detObj2D[objIndex].peakVal = (uint16_t)sqrtsp(temp);

            /* Calculate X and Y coordiantes in meters in Q8 format */
            range = obj->detObj2D[objIndex].rangeIdx * rangeResolution;

            if(azimIdx > (numAngleBins/2 -1))
            {
                sMaxIdx = azimIdx - numAngleBins;
            }
            else
            {
                sMaxIdx = azimIdx;
            }

            Wx = 2 * (float) sMaxIdx / numAngleBins;
            x = range * Wx;

            /* y = sqrt(range^2 - x^2) */
            temp = range*range -x*x ;
            if (temp > 0)
            {
                y = sqrtsp(temp);
            }
            else
            {
                y = 0;
            }

            /* Convert to Q8 format */
            obj->detObj2D[objIndex].x = (int16_t) ROUND(x * ONE_QFORMAT);
            obj->detObj2D[objIndex].y = (int16_t) ROUND(y * ONE_QFORMAT);
            obj->detObj2D[objIndex].z = 0;

        }
    }

}

/**
 *  @b Description
 *  @n
 *      Calculates Y coordinates in meters based on the maximum position in
 *      the magnitude square of the azimuth FFT output. This is called
 *      when the number of rx antennas is 1.
 *
 *  @param[in] obj                Pointer to data path object
 *
 *  @param[in] objIndex           Detected objet index
 *
 *  @retval
 *      NONE
 */
void MmwDemo_Yestimation(MmwDemo_DSS_DataPathObj *obj,
                          uint32_t objIndex)
{
    float range;
    float rangeResolution = obj->rangeResolution;
    uint32_t xyzOutputQFormat = obj->xyzOutputQFormat;
#define ONE_QFORMAT (1 << xyzOutputQFormat)

    /* Calculate Y coordiante in meters in Q8 format */
    range = obj->detObj2D[objIndex].rangeIdx * rangeResolution;

    /* Convert to Q8 format */
    obj->detObj2D[objIndex].x = 0;
    obj->detObj2D[objIndex].y = (int16_t) ROUND(range * ONE_QFORMAT);
    obj->detObj2D[objIndex].z = 0;
}

/**
 *  @b Description
 *  @n
 *      Waits for 1D FFT data to be transferrd to input buffer.
 *      This is a blocking function.
 *
 *  @param[in] obj  Pointer to data path object
 *  @param[in] pingPongId ping-pong id (ping is 0 and pong is 1)
 *
 *  @retval
 *      NONE
 */
void MmwDemo_dataPathWait1DInputData(MmwDemo_DSS_DataPathObj *obj, uint32_t pingPongId)
{
#ifdef EDMA_1D_INPUT_BLOCKING
    Bool       status;

    status = Semaphore_pend(obj->EDMA_1D_InputDone_semHandle[pingPongId], BIOS_WAIT_FOREVER);
    if (status != TRUE)
    {
        System_printf("Error: Semaphore_pend returned %d\n",status);
    }
#else
    /* wait until transfer done */
    volatile bool isTransferDone;
    uint8_t chId;
    if(pingPongId == 0)
    {
        chId = MMW_EDMA_CH_1D_IN_PING;
    }
    else
    {
        chId = MMW_EDMA_CH_1D_IN_PONG;
    }
    do {
        if (EDMA_isTransferComplete(obj->edmaHandle[EDMA_INSTANCE_A],
                                    chId,
                                    (bool *)&isTransferDone) != EDMA_NO_ERROR)
        {
            DebugP_assert(0);
        }
    } while (isTransferDone == false);
#endif
}

/**
 *  @b Description
 *  @n
 *      Waits for 1D FFT data to be transferred to output buffer.
 *      This is a blocking function.
 *
 *  @param[in] obj  Pointer to data path object
 *  @param[in] pingPongId ping-pong id (ping is 0 and pong is 1)
 *
 *  @retval
 *      NONE
 */
void MmwDemo_dataPathWait1DOutputData(MmwDemo_DSS_DataPathObj *obj, uint32_t pingPongId)
{
#ifdef EDMA_1D_OUTPUT_BLOCKING
    Bool       status;

    status = Semaphore_pend(obj->EDMA_1D_OutputDone_semHandle[pingPongId], BIOS_WAIT_FOREVER);
    if (status != TRUE)
    {
        System_printf("Error: Semaphore_pend returned %d\n",status);
    }
#else
    /* wait until transfer done */
    volatile bool isTransferDone;
    uint8_t chId;
    if(pingPongId == 0)
    {
        chId = MMW_EDMA_CH_1D_OUT_PING;
    }
    else
    {
        chId = MMW_EDMA_CH_1D_OUT_PONG;
    }
    do {
        if (EDMA_isTransferComplete(obj->edmaHandle[EDMA_INSTANCE_A],
                                    chId,
                                    (bool *)&isTransferDone) != EDMA_NO_ERROR)
        {
            DebugP_assert(0);
        }
    } while (isTransferDone == false);
#endif
}
/**
 *  @b Description
 *  @n
 *      Waits for 1D FFT data to be transferred to input buffer for 2D-FFT caclulation.
 *      This is a blocking function.
 *
 *  @param[in] obj  Pointer to data path object
 *  @param[in] pingPongId ping-pong id (ping is 0 and pong is 1)
 *
 *  @retval
 *      NONE
 */
void MmwDemo_dataPathWait2DInputData(MmwDemo_DSS_DataPathObj *obj, uint32_t pingPongId)
{
#ifdef EDMA_2D_INPUT_BLOCKING
    Bool       status;

    status = Semaphore_pend(obj->EDMA_2D_InputDone_semHandle[pingPongId], BIOS_WAIT_FOREVER);
    if (status != TRUE)
    {
        System_printf("Error: Semaphore_pend returned %d\n",status);
    }
#else
    /* wait until transfer done */
    volatile bool isTransferDone;
    uint8_t chId;
    if(pingPongId == 0)
    {
        chId = MMW_EDMA_CH_2D_IN_PING;
    }
    else
    {
        chId = MMW_EDMA_CH_2D_IN_PONG;
    }
    do {
        if (EDMA_isTransferComplete(obj->edmaHandle[EDMA_INSTANCE_A],
                                    chId,
                                    (bool *)&isTransferDone) != EDMA_NO_ERROR)
        {
            DebugP_assert(0);
        }
    } while (isTransferDone == false);
#endif
}
/**
 *  @b Description
 *  @n
 *      Waits for 1D FFT data to be transferred to input buffer for 3D-FFT calculation.
 *      This is a blocking function.
 *
 *  @param[in] obj  Pointer to data path object
 *  @param[in] pingPongId ping-pong id (ping is 0 and pong is 1)
 *
 *  @retval
 *      NONE
 */
void MmwDemo_dataPathWait3DInputData(MmwDemo_DSS_DataPathObj *obj, uint32_t pingPongId)
{
#ifdef EDMA_3D_INPUT_BLOCKING
    Bool       status;

    status = Semaphore_pend(obj->EDMA_3D_InputDone_semHandle[pingPongId], BIOS_WAIT_FOREVER);
    if (status != TRUE)
    {
        System_printf("Error: Semaphore_pend returned %d\n",status);
    }
#else
    /* wait until transfer done */
    volatile bool isTransferDone;
    uint8_t chId;
    if(pingPongId == 0)
    {
        chId = MMW_EDMA_CH_3D_IN_PING;
    }
    else
    {
        chId = MMW_EDMA_CH_3D_IN_PONG;
    }
    do {
        if (EDMA_isTransferComplete(obj->edmaHandle[EDMA_INSTANCE_A],
                                    chId,
                                    (bool *)&isTransferDone) != EDMA_NO_ERROR)
        {
            DebugP_assert(0);
        }
    } while (isTransferDone == false);
#endif
}

/**
 *  @b Description
 *  @n
 *      Waits for 2D FFT calculated data to be transferred out from L2 memory
 *      to detection matrix located in L3 memory.
 *      This is a blocking function.
 *
 *  @param[in] obj  Pointer to data path object
 *
 *  @retval
 *      NONE
 */
void MmwDemo_dataPathWaitTransDetMatrix(MmwDemo_DSS_DataPathObj *obj)
{
#ifdef EDMA_2D_OUTPUT_BLOCKING
    Bool       status;

    status = Semaphore_pend(obj->EDMA_DetMatrix_semHandle, BIOS_WAIT_FOREVER);
    if (status != TRUE)
    {
        System_printf("Error: Semaphore_pend returned %d\n",status);
    }
#else
    volatile bool isTransferDone;
    do {
        if (EDMA_isTransferComplete(obj->edmaHandle[EDMA_INSTANCE_A],
                                    (uint8_t) MMW_EDMA_CH_DET_MATRIX,
                                    (bool *)&isTransferDone) != EDMA_NO_ERROR)
        {
            DebugP_assert(0);
        }
    } while (isTransferDone == false);
#endif
}

/**
 *  @b Description
 *  @n
 *      Waits for 2D FFT data to be transferred from detection matrix in L3
 *      memory to L2 memory for CFAR detection in range direction.
 *      This is a blocking function.
 *
 *  @param[in] obj  Pointer to data path object
 *
 *  @retval
 *      NONE
 */
void MmwDemo_dataPathWaitTransDetMatrix2(MmwDemo_DSS_DataPathObj *obj)
{
#ifdef EDMA_3D_INPUT_BLOCKING
    Bool       status;

    status = Semaphore_pend(obj->EDMA_DetMatrix2_semHandle, BIOS_WAIT_FOREVER);
    if (status != TRUE)
    {
        System_printf("Error: Semaphore_pend returned %d\n",status);
    }
#else
    /* wait until transfer done */
    volatile bool isTransferDone;
    do {
        if (EDMA_isTransferComplete(obj->edmaHandle[EDMA_INSTANCE_A],
                                    (uint8_t) MMW_EDMA_CH_DET_MATRIX2,
                                    (bool *)&isTransferDone) != EDMA_NO_ERROR)
        {
            DebugP_assert(0);
        }
    } while (isTransferDone == false);
#endif
}

/**
 *  @b Description
 *  @n
 *      EDMA transfer completion call back function as per EDMA API.
 *      Depending on the programmed transfer completion codes,
 *      posts the corresponding done/completion semaphore.
 *      Per current design, a single semaphore could have been used as the
 *      1D, 2D and CFAR stages are sequential, this code gives some flexibility
 *      if some design change in future.
 */
void MmwDemo_EDMA_transferCompletionCallbackFxn(uintptr_t arg,
    uint32_t transferCompletionCode)
{
    MmwDemo_DSS_DataPathObj *obj = (MmwDemo_DSS_DataPathObj *)arg;

    switch (transferCompletionCode)
    {
        case MMW_EDMA_CH_1D_IN_PING:
            Semaphore_post(obj->EDMA_1D_InputDone_semHandle[0]);
        break;

        case MMW_EDMA_CH_1D_IN_PONG:
            Semaphore_post(obj->EDMA_1D_InputDone_semHandle[1]);
        break;

        case MMW_EDMA_CH_1D_OUT_PING:
            Semaphore_post(obj->EDMA_1D_OutputDone_semHandle[0]);
        break;

        case MMW_EDMA_CH_1D_OUT_PONG:
            Semaphore_post(obj->EDMA_1D_OutputDone_semHandle[1]);
        break;

        case MMW_EDMA_CH_2D_IN_PING:
            Semaphore_post(obj->EDMA_2D_InputDone_semHandle[0]);
        break;

        case MMW_EDMA_CH_2D_IN_PONG:
            Semaphore_post(obj->EDMA_2D_InputDone_semHandle[1]);
        break;

        case MMW_EDMA_CH_DET_MATRIX:
            Semaphore_post(obj->EDMA_DetMatrix_semHandle);
        break;

        case MMW_EDMA_CH_DET_MATRIX2:
            Semaphore_post(obj->EDMA_DetMatrix2_semHandle);
        break;

        case MMW_EDMA_CH_3D_IN_PING:
            Semaphore_post(obj->EDMA_3D_InputDone_semHandle[0]);
        break;

        case MMW_EDMA_CH_3D_IN_PONG:
            Semaphore_post(obj->EDMA_3D_InputDone_semHandle[1]);
        break;

        default:
            DebugP_assert(0);
        break;
    }
}

/**
 *  @b Description
 *  @n
 *      Configures all EDMA channels and param sets used in data path processing
 *  @param[in] obj  Pointer to data path object
 *
 *  @retval
 *      -1 if error, 0 for no error
 */
int32_t MmwDemo_dataPathConfigEdma(MmwDemo_DSS_DataPathObj *obj)
{
    uint32_t eventQueue;
    uint16_t shadowParam = EDMA_NUM_DMA_CHANNELS;
    int32_t retVal = 0;

    /*****************************************************
     * EDMA configuration for getting ADC data from ADC buffer
     * to L2 (prior to 1D FFT)
     * For ADC Buffer to L2 use EDMA-A TPTC =1
     *****************************************************/
    eventQueue = 0U;
     /* Ping - copies chirp samples from even antenna numbers (e.g. RxAnt0 and RxAnt2) */  
    retVal =
    EDMAutil_configType1(obj->edmaHandle[EDMA_INSTANCE_A],
        (uint8_t *)(&obj->ADCdataBuf[0]),
        (uint8_t *)(SOC_translateAddress((uint32_t)&obj->adcDataIn[0],SOC_TranslateAddr_Dir_TO_EDMA,NULL)),
        MMW_EDMA_CH_1D_IN_PING,
        false,
        shadowParam++,
        obj->numAdcSamples * BYTES_PER_SAMP_1D,
        MAX(obj->numRxAntennas / 2, 1),
        obj->numAdcSamples * BYTES_PER_SAMP_1D * 2,
        0,
        eventQueue,
#ifdef EDMA_1D_INPUT_BLOCKING
        MmwDemo_EDMA_transferCompletionCallbackFxn,
#else
        NULL,
#endif
        (uintptr_t) obj);
    if (retVal < 0)
    {
        return -1;
    }
    
    /* Pong - copies chirp samples from odd antenna numbers (e.g. RxAnt1 and RxAnt3) */
    retVal =
    EDMAutil_configType1(obj->edmaHandle[EDMA_INSTANCE_A],
        (uint8_t *)(&obj->ADCdataBuf[obj->numAdcSamples]),
        (uint8_t *)(SOC_translateAddress((uint32_t)(&obj->adcDataIn[obj->numRangeBins]),SOC_TranslateAddr_Dir_TO_EDMA,NULL)),
        MMW_EDMA_CH_1D_IN_PONG,
        false,
        shadowParam++,
        obj->numAdcSamples * BYTES_PER_SAMP_1D,
        MAX(obj->numRxAntennas / 2, 1),
        obj->numAdcSamples * BYTES_PER_SAMP_1D * 2,
        0,
        eventQueue,
#ifdef EDMA_1D_INPUT_BLOCKING
        MmwDemo_EDMA_transferCompletionCallbackFxn,
#else
        NULL,
#endif
        (uintptr_t) obj);
    if (retVal < 0)
    {
        return -1;
    }
    
    /* using different event queue between input and output to parallelize better */
    eventQueue = 1U;
    /*****************************************************
     * EDMA configuration for storing 1d fft output in transposed manner to L3.
     * It copies all Rx antennas of the chirp per trigger event.
     *****************************************************/
    /* Ping - Copies from ping FFT output (even chirp indices)  to L3 */
    retVal =
    EDMAutil_configType2a(obj->edmaHandle[EDMA_INSTANCE_A],
        (uint8_t *)(SOC_translateAddress((uint32_t)(&obj->fftOut1D[0]),SOC_TranslateAddr_Dir_TO_EDMA,NULL)),
        (uint8_t *)(&obj->radarCube[0]),
        MMW_EDMA_CH_1D_OUT_PING,
        false,
        shadowParam++,
        BYTES_PER_SAMP_1D,
        obj->numRangeBins,
        obj->numTxAntennas,
        obj->numRxAntennas,
        obj->numDopplerBins,
        eventQueue,
#ifdef EDMA_1D_OUTPUT_BLOCKING
        MmwDemo_EDMA_transferCompletionCallbackFxn,
#else
        NULL,
#endif
        (uintptr_t) obj);
    if (retVal < 0)
    {
        return -1;
    }

    /* Ping - Copies from pong FFT output (odd chirp indices)  to L3 */
    retVal =
    EDMAutil_configType2a(obj->edmaHandle[EDMA_INSTANCE_A],
        (uint8_t *)(SOC_translateAddress((uint32_t)(&obj->fftOut1D[obj->numRxAntennas * obj->numRangeBins]),
                                         SOC_TranslateAddr_Dir_TO_EDMA,NULL)),
        (uint8_t *)(&obj->radarCube[0]),
        MMW_EDMA_CH_1D_OUT_PONG,
        false,
        shadowParam++,
        BYTES_PER_SAMP_1D,
        obj->numRangeBins,
        obj->numTxAntennas,
        obj->numRxAntennas,
        obj->numDopplerBins,
        eventQueue,
#ifdef EDMA_1D_OUTPUT_BLOCKING
        MmwDemo_EDMA_transferCompletionCallbackFxn,
#else
        NULL,
#endif
        (uintptr_t) obj);
    if (retVal < 0)
    {
        return -1;
    }

    /*****************************************
     * Interframe processing related EDMA configuration
     *****************************************/
    eventQueue = 0U;
    /* Ping: This DMA channel is programmed to fetch the 1D FFT data from radarCube
     * matrix in L3 mem of even antenna rows into the Ping Buffer in L2 mem*/
    retVal =
    EDMAutil_configType1(obj->edmaHandle[EDMA_INSTANCE_A],
        (uint8_t *)(&obj->radarCube[0]),
        (uint8_t *)(SOC_translateAddress((uint32_t)(&obj->dstPingPong[0]),SOC_TranslateAddr_Dir_TO_EDMA,NULL)),
        MMW_EDMA_CH_2D_IN_PING,
        false,
        shadowParam++,
        obj->numDopplerBins * BYTES_PER_SAMP_1D,
        (obj->numRangeBins * obj->numRxAntennas * obj->numTxAntennas) / 2,
        obj->numDopplerBins * BYTES_PER_SAMP_1D * 2,
        0,
        eventQueue,
#ifdef EDMA_2D_INPUT_BLOCKING
        MmwDemo_EDMA_transferCompletionCallbackFxn,
#else
        NULL,
#endif
        (uintptr_t) obj);
    if (retVal < 0)
    {
        return -1;
    }

    /* Pong: This DMA channel is programmed to fetch the 1D FFT data from radarCube
     * matrix in L3 mem of odd antenna rows into thePong Buffer in L2 mem*/
    retVal =
    EDMAutil_configType1(obj->edmaHandle[EDMA_INSTANCE_A],
        (uint8_t *)(&obj->radarCube[obj->numDopplerBins]),
        (uint8_t *)(SOC_translateAddress((uint32_t)(&obj->dstPingPong[obj->numDopplerBins]),
                                          SOC_TranslateAddr_Dir_TO_EDMA,NULL)),
        MMW_EDMA_CH_2D_IN_PONG,
        false,
        shadowParam++,
        obj->numDopplerBins * BYTES_PER_SAMP_1D,
        (obj->numRangeBins * obj->numRxAntennas * obj->numTxAntennas) / 2,
        obj->numDopplerBins * BYTES_PER_SAMP_1D * 2,
        0,
        eventQueue,
#ifdef EDMA_2D_INPUT_BLOCKING
        MmwDemo_EDMA_transferCompletionCallbackFxn,
#else
        NULL,
#endif
        (uintptr_t) obj);
    if (retVal < 0)
    {
        return -1;
    }

    eventQueue = 1U;
    /* This EDMA channel copes the sum (across virtual antennas) of log2
     * magnitude squared of Doppler FFT bins from L2 mem to detection
     * matrix in L3 mem */
    retVal =
    EDMAutil_configType1(obj->edmaHandle[EDMA_INSTANCE_A],
        (uint8_t *)(SOC_translateAddress((uint32_t)(&obj->sumAbs[0U]),SOC_TranslateAddr_Dir_TO_EDMA,NULL)),
        (uint8_t *)(obj->detMatrix),
        MMW_EDMA_CH_DET_MATRIX,
        false,
        shadowParam++,
        obj->numDopplerBins * BYTES_PER_SAMP_DET,
        obj->numRangeBins,
        0,
        obj->numDopplerBins * BYTES_PER_SAMP_DET,
        eventQueue,
#ifdef EDMA_2D_OUTPUT_BLOCKING
        MmwDemo_EDMA_transferCompletionCallbackFxn,
#else
        NULL,
#endif
        (uintptr_t) obj);
    if (retVal < 0)
    {
        return -1;
    }

    /* This EDMA Channel brings selected range bins  from detection matrix in
     * L3 mem (reading in transposed manner) into L2 mem for CFAR detection (in
     * range direction) */
    retVal =
    EDMAutil_configType3(obj->edmaHandle[EDMA_INSTANCE_A],
        (uint8_t *)0,
        (uint8_t *)0,
        MMW_EDMA_CH_DET_MATRIX2,
        false,
        shadowParam++,
        BYTES_PER_SAMP_DET,\
        obj->numRangeBins,
        obj->numDopplerBins * BYTES_PER_SAMP_DET,
        BYTES_PER_SAMP_DET,
        eventQueue,
#ifdef EDMA_MATRIX2_INPUT_BLOCKING
        MmwDemo_EDMA_transferCompletionCallbackFxn,
#else
        NULL,
#endif
        (uintptr_t) obj);
    if (retVal < 0)
    {
        return -1;
    }

    /*********************************************************
     * These EDMA Channels are for Azimuth calculation. They bring
     * 1D FFT data for 2D DFT and Azimuth FFT calculation.
     ********************************************************/
    /* Ping: This DMA channel is programmed to fetch the 1D FFT data from radarCube
     * matrix in L3 mem of even antenna rows into the Ping Buffer in L2 mem.
     * */
    retVal =
    EDMAutil_configType1(obj->edmaHandle[EDMA_INSTANCE_A],
        (uint8_t *) NULL,
        (uint8_t *)(SOC_translateAddress((uint32_t)(&obj->dstPingPong[0]),SOC_TranslateAddr_Dir_TO_EDMA,NULL)),
        MMW_EDMA_CH_3D_IN_PING,
        false,
        shadowParam++,
        obj->numDopplerBins * BYTES_PER_SAMP_1D,
        MAX((obj->numRxAntennas * obj->numTxAntennas) / 2, 1),
        obj->numDopplerBins * BYTES_PER_SAMP_1D * 2,
        0,
        eventQueue,
#ifdef EDMA_3D_INPUT_BLOCKING
        MmwDemo_EDMA_transferCompletionCallbackFxn,
#else
        NULL,
#endif
        (uintptr_t) obj);
    if (retVal < 0)
    {
        return -1;
    }

    /* Pong: This DMA channel is programmed to fetch the 1D FFT data from radarCube
     * matrix in L3 mem of odd antenna rows into the Pong Buffer in L2 mem*/
    retVal =
    EDMAutil_configType1(obj->edmaHandle[EDMA_INSTANCE_A],
        (uint8_t *) NULL,
        (uint8_t *)(SOC_translateAddress((uint32_t)(&obj->dstPingPong[obj->numDopplerBins]),SOC_TranslateAddr_Dir_TO_EDMA,NULL)),
        MMW_EDMA_CH_3D_IN_PONG,
        false,
        shadowParam++,
        obj->numDopplerBins * BYTES_PER_SAMP_1D,
        MAX((obj->numRxAntennas * obj->numTxAntennas) / 2, 1),
        obj->numDopplerBins * BYTES_PER_SAMP_1D * 2,
        0,
        eventQueue,
#ifdef EDMA_3D_INPUT_BLOCKING
        MmwDemo_EDMA_transferCompletionCallbackFxn,
#else
        NULL,
#endif
        (uintptr_t) obj);
    if (retVal < 0)
    {
        return -1;
    }

    return(0);
}

/**
 *  @b Description
 *  @n
 *    The function groups neighboring peaks into one. The grouping is done
 *    according to two input flags: groupInDopplerDirection and
 *    groupInDopplerDirection. For each detected peak the function checks
 *    if the peak is greater than its neighbors. If this is true, the peak is
 *    copied to the output list of detected objects. The neighboring peaks that are used
 *    for checking are taken from the detection matrix and copied into 3x3 kernel
 *    regardless of whether they are CFAR detected or not.
 *    Note: Function always reads 9 samples per detected object
 *    from L3 memory into local array tempBuff, but it only needs to read according to input flags.
 *    For example if only the groupInDopplerDirection flag is set, it only needs
 *    to read middle row of the kernel, i.e. 3 samples per target from detection matrix.
 *  @param[out]   objOut             Output array of  detected objects after peak grouping
 *  @param[in]    objRaw             Array of detected objects after CFAR detection
 *  @param[in]    numDetectedObjects Number of detected objects by CFAR
 *  @param[in]    detMatrix          Detection Range/Doppler matrix
 *  @param[in]    numDopplerBins     Number of Doppler bins
 *  @param[in]    maxRangeIdx        Maximum range limit
 *  @param[in]    minRangeIdx        Minimum range limit
 *  @param[in]    groupInDopplerDirection Flag enables grouping in Doppler directiuon
 *  @param[in]    groupInRangeDirection   Flag enables grouping in Range directiuon
 *
 *  @retval
 *      Number of detected objects after grouping
 */
uint32_t MmwDemo_cfarPeakGrouping(
                                MmwDemo_detectedObj*  objOut,
                                MmwDemo_objRaw_t * objRaw,
                                uint32_t numDetectedObjects,
                                uint16_t* detMatrix,
                                uint32_t numDopplerBins,
                                uint32_t maxRangeIdx,
                                uint32_t minRangeIdx,
                                uint32_t groupInDopplerDirection,
                                uint32_t groupInRangeDirection)
{
    int32_t i, j;
    int32_t rowStart, rowEnd;
    uint32_t numObjOut = 0;
    uint32_t rangeIdx, dopplerIdx, peakVal;
    uint16_t *tempPtr;
    uint16_t kernel[9], detectedObjFlag;
    int32_t k, l;
    uint32_t startInd, stepInd, endInd;

    if ((groupInDopplerDirection == 1) && (groupInRangeDirection == 1))
    {
        /* Grouping both in Range and Doppler direction */
        startInd = 0;
        stepInd = 1;
        endInd = 8;
    }
    else if ((groupInDopplerDirection == 0) && (groupInRangeDirection == 1))
    {
        /* Grouping only in Range direction */
        startInd = 1;
        stepInd = 3;
        endInd = 7;
    }
    else if ((groupInDopplerDirection == 1) && (groupInRangeDirection == 0))
    {
        /* Grouping only in Doppler direction */
        startInd = 3;
        stepInd = 1;
        endInd = 5;
    }
    else
    {
        /* No grouping, copy all detected objects to the output matrix within specified min max range*/
        for(i = 0; i < MIN(numDetectedObjects, MMW_MAX_OBJ_OUT) ; i++)
        {
            if ((objRaw[i].rangeIdx <= maxRangeIdx) && ((objRaw[i].rangeIdx >= minRangeIdx)))
            {
                objOut[numObjOut].rangeIdx = objRaw[i].rangeIdx;
                objOut[numObjOut].dopplerIdx = objRaw[i].dopplerIdx;
                objOut[numObjOut].peakVal = objRaw[i].peakVal;
                numObjOut++;
            }
        }
        return numObjOut;
    }

    /* Start checking */
    for(i = 0; i < numDetectedObjects; i++)
    {
        detectedObjFlag = 0;
        rangeIdx = objRaw[i].rangeIdx;
        dopplerIdx = objRaw[i].dopplerIdx;
        peakVal = objRaw[i].peakVal;
        if((rangeIdx <= maxRangeIdx) && (rangeIdx >= minRangeIdx))
        {
            detectedObjFlag = 1;

            /* Fill local 3x3 kernel from detection matrix in L3*/
            tempPtr = detMatrix + (rangeIdx-1)*numDopplerBins;
            rowStart = 0;
            rowEnd = 2;
            if (rangeIdx == minRangeIdx)
            {
                tempPtr = detMatrix + (rangeIdx)*numDopplerBins;
                rowStart = 1;
                memset((void *) kernel, 0, 3 * sizeof(uint16_t));
            }
            else if (rangeIdx == maxRangeIdx)
            {
                rowEnd = 1;
                memset((void *) &kernel[6], 0, 3 * sizeof(uint16_t));
            }

            for (j = rowStart; j <= rowEnd; j++)
            {
                for (k = 0; k < 3; k++)
                {
                    l = dopplerIdx + (k - 1);
                    if(l < 0)
                    {
                        l += numDopplerBins;
                    }
                    else if(l >= numDopplerBins)
                    {
                        l -= numDopplerBins;
                    }
                    kernel[j*3+k] = tempPtr[l];
                }
                tempPtr += numDopplerBins;
            }
            /* Compare the detected object to its neighbors.
             * Detected object is at index 4*/
            for (k = startInd; k <= endInd; k += stepInd)
            {
                if(kernel[k] > kernel[4])
                {
                    detectedObjFlag = 0;
                }
            }
        }
        if (detectedObjFlag == 1)
        {
            objOut[numObjOut].rangeIdx = rangeIdx;
            objOut[numObjOut].dopplerIdx = dopplerIdx;
            objOut[numObjOut].peakVal = peakVal;
            numObjOut++;
        }
        if (numObjOut >= MMW_MAX_OBJ_OUT)
        {
            break;
        }

    }

    return(numObjOut);
}


/**
 *  @b Description
 *  @n
 *    The function groups neighboring peaks into one. The grouping is done
 *    according to two input flags: groupInDopplerDirection and
 *    groupInDopplerDirection. For each detected peak the function checks
 *    if the peak is greater than its neighbors. If this is true, the peak is
 *    copied to the output list of detected objects. The neighboring peaks that
 *    are used for checking are taken from the list of CFAR detected objects,
 *    (not from the detection matrix), and copied into 3x3 kernel that has been
 *    initialized to zero for each peak under test. If the neighboring cell has
 *    not been detected by CFAR, its peak value is not copied into the kernel.
 *    Note: Function always search for 8 peaks in the list, but it only needs to search
 *    according to input flags.
 *  @param[out]   objOut             Output array of  detected objects after peak grouping
 *  @param[in]    objRaw             Array of detected objects after CFAR detection
 *  @param[in]    numDetectedObjects Number of detected objects by CFAR
 *  @param[in]    numDopplerBins     Number of Doppler bins
 *  @param[in]    maxRangeIdx        Maximum range limit
 *  @param[in]    minRangeIdx        Minimum range limit
 *  @param[in]    groupInDopplerDirection Flag enables grouping in Doppler directiuon
 *  @param[in]    groupInRangeDirection   Flag enables grouping in Range directiuon
 *
 *  @retval
 *      Number of detected objects after grouping
 */
uint32_t MmwDemo_cfarPeakGroupingCfarQualified(
                                MmwDemo_detectedObj*  objOut,
                                MmwDemo_objRaw_t * objRaw,
                                uint32_t numDetectedObjects,
                                uint32_t numDopplerBins,
                                uint32_t maxRangeIdx,
                                uint32_t minRangeIdx,
                                uint32_t groupInDopplerDirection,
                                uint32_t groupInRangeDirection)
{
    int32_t i;
    uint32_t numObjOut = 0;
    uint32_t rangeIdx, dopplerIdx, peakVal;
    uint16_t kernel[9], detectedObjFlag;
    int32_t k;
    int32_t l;
    uint32_t startInd, stepInd, endInd;

#define WRAP_DOPPLER_IDX(_x) ((_x) & (numDopplerBins-1))
#define WRAP_DWN_LIST_IDX(_x) ((_x) >= numDetectedObjects ? ((_x) - numDetectedObjects) : (_x))
#define WRAP_UP_LIST_IDX(_x) ((_x) < 0 ? ((_x) + numDetectedObjects) : (_x))

    if ((groupInDopplerDirection == 1) && (groupInRangeDirection == 1))
    {
        /* Grouping both in Range and Doppler direction */
        startInd = 0;
        stepInd = 1;
        endInd = 8;
    }
    else if ((groupInDopplerDirection == 0) && (groupInRangeDirection == 1))
    {
        /* Grouping only in Range direction */
        startInd = 1;
        stepInd = 3;
        endInd = 7;
    }
    else if ((groupInDopplerDirection == 1) && (groupInRangeDirection == 0))
    {
        /* Grouping only in Doppler direction */
        startInd = 3;
        stepInd = 1;
        endInd = 5;
    }
    else
    {
        /* No grouping, copy all detected objects to the output matrix */
        for(i = 0; i < MIN(numDetectedObjects, MMW_MAX_OBJ_OUT) ; i++)
        {
            if ((objRaw[i].rangeIdx <= maxRangeIdx) && ((objRaw[i].rangeIdx >= minRangeIdx)))
            {
                objOut[numObjOut].rangeIdx = objRaw[i].rangeIdx;;
                objOut[numObjOut].dopplerIdx = objRaw[i].dopplerIdx;
                objOut[numObjOut].peakVal = objRaw[i].peakVal;
                numObjOut++;
            }
        }
        return numObjOut;
    }

    /* Start checking  */
    for(i = 0; i < numDetectedObjects ; i++)
    {
        detectedObjFlag = 0;
        rangeIdx = objRaw[i].rangeIdx;
        dopplerIdx = objRaw[i].dopplerIdx;
        peakVal = objRaw[i].peakVal;

        if((rangeIdx <= maxRangeIdx) && (rangeIdx >= minRangeIdx))
        {
            detectedObjFlag = 1;
            memset((void *) kernel, 0, 9*sizeof(uint16_t));

            /* Fill the middle column of the kernel */
            kernel[4] = peakVal;

            if (i > 0)
            {
                if ((objRaw[i-1].rangeIdx == (rangeIdx-1)) &&
                    (objRaw[i-1].dopplerIdx == dopplerIdx))
                {
                    kernel[1] = objRaw[i-1].peakVal;
                }
            }

            if (i < (numDetectedObjects - 1))
            {
                if ((objRaw[i+1].rangeIdx == (rangeIdx+1)) &&
                    (objRaw[i+1].dopplerIdx == dopplerIdx))
                {
                    kernel[7] = objRaw[i+1].peakVal;
                }
            }

            /* Fill the left column of the kernel */
            k = WRAP_UP_LIST_IDX(i-1);
            for (l = 0; l < numDetectedObjects; l++)
            {
                if (objRaw[k].dopplerIdx == WRAP_DOPPLER_IDX(dopplerIdx - 2))
                {
                    break;
                }
                if ((objRaw[k].rangeIdx == (rangeIdx + 1)) &&
                    (objRaw[k].dopplerIdx == WRAP_DOPPLER_IDX(dopplerIdx - 1)))
                {
                    kernel[6] = objRaw[k].peakVal;
                }
                else if ((objRaw[k].rangeIdx == (rangeIdx)) &&
                    (objRaw[k].dopplerIdx == WRAP_DOPPLER_IDX(dopplerIdx - 1)))
                {
                    kernel[3] = objRaw[k].peakVal;
                }
                else if ((objRaw[k].rangeIdx == (rangeIdx - 1)) &&
                    (objRaw[k].dopplerIdx == WRAP_DOPPLER_IDX(dopplerIdx - 1)))
                {
                    kernel[0] = objRaw[k].peakVal;
                }
                k = WRAP_UP_LIST_IDX(k-1);
            }

            /* Fill the right column of the kernel */
            k = WRAP_DWN_LIST_IDX(i+1);
            for (l = 0; l < numDetectedObjects; l++)
            {
                if (objRaw[k].dopplerIdx == WRAP_DOPPLER_IDX(dopplerIdx + 2))
                {
                    break;
                }
                if ((objRaw[k].rangeIdx == (rangeIdx - 1)) &&
                    (objRaw[k].dopplerIdx == WRAP_DOPPLER_IDX(dopplerIdx + 1)))
                {
                    kernel[2] = objRaw[k].peakVal;
                }
                else if ((objRaw[k].rangeIdx == (rangeIdx)) &&
                    (objRaw[k].dopplerIdx == WRAP_DOPPLER_IDX(dopplerIdx + 1)))
                {
                    kernel[5] = objRaw[k].peakVal;
                }
                else if ((objRaw[k].rangeIdx == (rangeIdx + 1)) &&
                    (objRaw[k].dopplerIdx == WRAP_DOPPLER_IDX(dopplerIdx + 1)))
                {
                    kernel[8] = objRaw[k].peakVal;
                }
                k = WRAP_DWN_LIST_IDX(k+1);
            }

            /* Compare the detected object to its neighbors.
             * Detected object is at index 4*/
            for (k = startInd; k <= endInd; k += stepInd)
            {
                if(kernel[k] > kernel[4])
                {
                    detectedObjFlag = 0;
                }
            }
        }
        if(detectedObjFlag == 1)
        {
            objOut[numObjOut].rangeIdx = rangeIdx;
            objOut[numObjOut].dopplerIdx = dopplerIdx;
            objOut[numObjOut].peakVal = peakVal;
            numObjOut++;
        }
        if (numObjOut >= MMW_MAX_OBJ_OUT)
        {
            break;
        }
    }

    return(numObjOut);
}

/**
 *  @b Description
 *  @n
 *    Outputs magnitude squared float array of input complex32 array
 *
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_magnitudeSquared(cmplx32ReIm_t * restrict inpBuff, float * restrict magSqrdBuff, uint32_t numSamples)
{
    uint32_t i;
    for (i = 0; i < numSamples; i++)
    {
        magSqrdBuff[i] = (float) inpBuff[i].real * (float) inpBuff[i].real +
                (float) inpBuff[i].imag * (float) inpBuff[i].imag;
    }
}


#define pingPongId(x) ((x) & 0x1U)
#define isPong(x) (pingPongId(x))

/**
 *  @b Description
 *  @n
 *    Compensation of DC range antenna signature
 *    
 *
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_dcRangeSignatureCompensation(MmwDemo_DSS_DataPathObj *obj,  uint8_t chirpPingPongId)
{
    uint32_t rxAntIdx, binIdx;
    uint32_t ind;
    int32_t chirpPingPongOffs;
    int32_t chirpPingPongSize;

    chirpPingPongSize = obj->numRxAntennas * (obj->calibDcRangeSigCfg.positiveBinIdx - obj->calibDcRangeSigCfg.negativeBinIdx + 1);
    if (obj->dcRangeSigCalibCntr == 0)
    {
        memset(obj->dcRangeSigMean, 0, obj->numTxAntennas * chirpPingPongSize * sizeof(cmplx32ImRe_t));
    }

    chirpPingPongOffs = chirpPingPongId * chirpPingPongSize;

    /* Calibration */
    if (obj->dcRangeSigCalibCntr < (obj->calibDcRangeSigCfg.numAvgChirps * obj->numTxAntennas))
    {
        /* Accumulate */
        ind = 0;
        for (rxAntIdx  = 0; rxAntIdx < obj->numRxAntennas; rxAntIdx++)
        {
            uint32_t chirpInOffs = chirpPingPongId * (obj->numRxAntennas * obj->numRangeBins) + (obj->numRangeBins * rxAntIdx);
            int64_t *meanPtr = (int64_t *) &obj->dcRangeSigMean[chirpPingPongOffs];
            uint32_t *fftPtr =  (uint32_t *) &obj->fftOut1D[chirpInOffs];
            int64_t meanBin;
            uint32_t fftBin;
            int32_t Re, Im;
            for (binIdx = 0; binIdx <= obj->calibDcRangeSigCfg.positiveBinIdx; binIdx++)
            {
                meanBin = _amem8(&meanPtr[ind]);
                fftBin = _amem4(&fftPtr[binIdx]);
                Im = _loll(meanBin) + _ext(fftBin, 0, 16);
                Re = _hill(meanBin) + _ext(fftBin, 16, 16);
                _amem8(&meanPtr[ind]) = _itoll(Re, Im);
                ind++;
            }

            chirpInOffs = chirpPingPongId * (obj->numRxAntennas * obj->numRangeBins) + (obj->numRangeBins * rxAntIdx) + obj->numRangeBins + obj->calibDcRangeSigCfg.negativeBinIdx;
            fftPtr =  (uint32_t *) &obj->fftOut1D[chirpInOffs];
            for (binIdx = 0; binIdx < -obj->calibDcRangeSigCfg.negativeBinIdx; binIdx++)
            {
                meanBin = _amem8(&meanPtr[ind]);
                fftBin = _amem4(&fftPtr[binIdx]);
                Im = _loll(meanBin) + _ext(fftBin, 0, 16);
                Re = _hill(meanBin) + _ext(fftBin, 16, 16);
                _amem8(&meanPtr[ind]) = _itoll(Re, Im);
                ind++;
            }
        }
        obj->dcRangeSigCalibCntr++;

        if (obj->dcRangeSigCalibCntr == (obj->calibDcRangeSigCfg.numAvgChirps * obj->numTxAntennas))
        {
            /* Divide */
            int64_t *meanPtr = (int64_t *) obj->dcRangeSigMean;
            int32_t Re, Im;
            int64_t meanBin;
            int32_t divShift = obj->log2NumAvgChirps;
            for (ind  = 0; ind < (obj->numTxAntennas * chirpPingPongSize); ind++)
            {
                meanBin = _amem8(&meanPtr[ind]);
                Im = _sshvr(_loll(meanBin), divShift);
                Re = _sshvr(_hill(meanBin), divShift);
                _amem8(&meanPtr[ind]) = _itoll(Re, Im);
            }
        }
    }
    else
    {
       /* fftOut1D -= dcRangeSigMean */
        ind = 0;
        for (rxAntIdx  = 0; rxAntIdx < obj->numRxAntennas; rxAntIdx++)
        {
            uint32_t chirpInOffs = chirpPingPongId * (obj->numRxAntennas * obj->numRangeBins) + (obj->numRangeBins * rxAntIdx);
            int64_t *meanPtr = (int64_t *) &obj->dcRangeSigMean[chirpPingPongOffs];
            uint32_t *fftPtr =  (uint32_t *) &obj->fftOut1D[chirpInOffs];
            int64_t meanBin;
            uint32_t fftBin;
            int32_t Re, Im;
            for (binIdx = 0; binIdx <= obj->calibDcRangeSigCfg.positiveBinIdx; binIdx++)
            {
                meanBin = _amem8(&meanPtr[ind]);
                fftBin = _amem4(&fftPtr[binIdx]);
                Im = _ext(fftBin, 0, 16) - _loll(meanBin);
                Re = _ext(fftBin, 16, 16) - _hill(meanBin);
                _amem4(&fftPtr[binIdx]) = _pack2(Im, Re);
                ind++;
            }

            chirpInOffs = chirpPingPongId * (obj->numRxAntennas * obj->numRangeBins) + (obj->numRangeBins * rxAntIdx) + obj->numRangeBins + obj->calibDcRangeSigCfg.negativeBinIdx;
            fftPtr =  (uint32_t *) &obj->fftOut1D[chirpInOffs];
            for (binIdx = 0; binIdx < -obj->calibDcRangeSigCfg.negativeBinIdx; binIdx++)
            {
                meanBin = _amem8(&meanPtr[ind]);
                fftBin = _amem4(&fftPtr[binIdx]);
                Im = _ext(fftBin, 0, 16) - _loll(meanBin);
                Re = _ext(fftBin, 16, 16) - _hill(meanBin);
                _amem4(&fftPtr[binIdx]) = _pack2(Im, Re);
                //_amem4(&fftPtr[binIdx]) = _packh2(_sshvl(Im,16) , _sshvl(Re, 16));
                ind++;
            }

        }
    }
}


/**
 *  @b Description
 *  @n
 *    Interchirp processing. It is executed per chirp event, after ADC
 *    buffer is filled with chirp samples.
 *
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_interChirpProcessing(MmwDemo_DSS_DataPathObj *obj, uint8_t chirpPingPongId)
{
    uint32_t antIndx, waitingTime;
    volatile uint32_t startTime;
    volatile uint32_t startTime1;

    waitingTime = 0;
    startTime = Cycleprofiler_getTimeStamp();

    /* Kick off DMA to fetch data from ADC buffer for first channel */
    EDMA_startDmaTransfer(obj->edmaHandle[EDMA_INSTANCE_A],
                       MMW_EDMA_CH_1D_IN_PING);

    /* 1d fft for first antenna, followed by kicking off the DMA of fft output */
    for (antIndx = 0; antIndx < obj->numRxAntennas; antIndx++)
    {
        /* kick off DMA to fetch data for next antenna */
        if (antIndx < (obj->numRxAntennas - 1))
        {
            if (isPong(antIndx))
            {
                EDMA_startDmaTransfer(obj->edmaHandle[EDMA_INSTANCE_A],
                        MMW_EDMA_CH_1D_IN_PING);
            }
            else
            {
                EDMA_startDmaTransfer(obj->edmaHandle[EDMA_INSTANCE_A],
                        MMW_EDMA_CH_1D_IN_PONG);
            }
        }

        /* verify if DMA has completed for current antenna */
        startTime1 = Cycleprofiler_getTimeStamp();
        MmwDemo_dataPathWait1DInputData (obj, pingPongId(antIndx));
        waitingTime += Cycleprofiler_getTimeStamp() - startTime1;

        mmwavelib_windowing16x16(
                (int16_t *) &obj->adcDataIn[pingPongId(antIndx) * obj->numRangeBins],
                (int16_t *) obj->window1D,
                obj->numAdcSamples);
        memset((void *)&obj->adcDataIn[pingPongId(antIndx) * obj->numRangeBins + obj->numAdcSamples], 
            0 , (obj->numRangeBins - obj->numAdcSamples) * sizeof(cmplx16ReIm_t));
        DSP_fft16x16(
                (int16_t *) obj->twiddle16x16_1D,
                obj->numRangeBins,
                (int16_t *) &obj->adcDataIn[pingPongId(antIndx) * obj->numRangeBins],
                (int16_t *) &obj->fftOut1D[chirpPingPongId * (obj->numRxAntennas * obj->numRangeBins) +
                    (obj->numRangeBins * antIndx)]);

    }

    if(obj->calibDcRangeSigCfg.enabled)
    {
        MmwDemo_dcRangeSignatureCompensation(obj, chirpPingPongId);
    }

    gCycleLog.interChirpProcessingTime += Cycleprofiler_getTimeStamp() - startTime - waitingTime;
    gCycleLog.interChirpWaitTime += waitingTime;
}



/**
 *  @b Description
 *  @n
 *    Interframe processing. It is called from MmwDemo_dssDataPathProcessEvents
 *    after all chirps of the frame have been received and 1D FFT processing on them
 *    has been completed.
 *
 *  @retval
 *      Not Applicable.
 */

void MmwDemo_interFrameProcessing(MmwDemo_DSS_DataPathObj *obj)
{
    uint32_t rangeIdx, idx, detIdx1, detIdx2, numDetObjPerCfar, numDetObj1D, numDetObj2D;
    int32_t rxAntIdx;
    volatile uint32_t startTime;
    volatile uint32_t startTimeWait;
    uint32_t waitingTime = 0;
    uint32_t binIndex = 0;
    uint32_t pingPongIdx = 0;
    uint32_t dopplerLine, dopplerLineNext;
    startTime = Cycleprofiler_getTimeStamp();

    /* trigger first DMA */
    EDMA_startDmaTransfer(obj->edmaHandle[EDMA_INSTANCE_A], MMW_EDMA_CH_2D_IN_PING);

    /* initialize the  variable that keeps track of the number of objects detected */
    numDetObj1D = 0;
    MmwDemo_resetDopplerLines(&obj->detDopplerLines);
    for (rangeIdx = 0; rangeIdx < obj->numRangeBins; rangeIdx++)
    {
        /* 2nd Dimension FFT is done here */
        for (rxAntIdx = 0; rxAntIdx < (obj->numRxAntennas * obj->numTxAntennas); rxAntIdx++)
        {
            /* verify that previous DMA has completed */
            startTimeWait = Cycleprofiler_getTimeStamp();
            MmwDemo_dataPathWait2DInputData (obj, pingPongId(pingPongIdx));
            waitingTime += Cycleprofiler_getTimeStamp() - startTimeWait;

            /* kick off next DMA */
            if ((rangeIdx < obj->numRangeBins - 1) || (rxAntIdx < (obj->numRxAntennas * obj->numTxAntennas) - 1))
            {
                if (isPong(pingPongIdx))
                {
                    EDMA_startDmaTransfer(obj->edmaHandle[EDMA_INSTANCE_A], MMW_EDMA_CH_2D_IN_PING);
                }
                else
                {
                    EDMA_startDmaTransfer(obj->edmaHandle[EDMA_INSTANCE_A], MMW_EDMA_CH_2D_IN_PONG);
                }
            }

            /* process data that has just been DMA-ed  */
            mmwavelib_windowing16x32(
                  (int16_t *) &obj->dstPingPong[pingPongId(pingPongIdx) * obj->numDopplerBins],
                  obj->window2D,
                  (int32_t *) obj->windowingBuf2D,
                  obj->numDopplerBins);

            DSP_fft32x32(
                        (int32_t *)obj->twiddle32x32_2D,
                        obj->numDopplerBins,
                        (int32_t *) obj->windowingBuf2D,
                        (int32_t *) obj->fftOut2D);

            /* Save only for static azimuth heatmap display, scale to 16-bit precision  */
            obj->azimuthStaticHeatMap[binIndex].real = (int16_t) (obj->fftOut2D[0].real >> (obj->log2NumDopplerBins+4)); /* +4 since 2D-FFT window has gain of 2^4 */
            obj->azimuthStaticHeatMap[binIndex++].imag = (int16_t) (obj->fftOut2D[0].imag >> (obj->log2NumDopplerBins+4));


            mmwavelib_log2Abs32(
                        (int32_t *) obj->fftOut2D,
                        obj->log2Abs,
                        obj->numDopplerBins);

            if (rxAntIdx == 0)
            {
                /* check if previous  sumAbs has been transferred */
                if (rangeIdx > 0)
                {
                    startTimeWait = Cycleprofiler_getTimeStamp();
                    MmwDemo_dataPathWaitTransDetMatrix (obj);
                    waitingTime += Cycleprofiler_getTimeStamp() - startTimeWait;
                }

                for (idx = 0; idx < obj->numDopplerBins; idx++)
                {
                    obj->sumAbs[idx] = obj->log2Abs[idx];
                }
            }
            else
            {
                mmwavelib_accum16(obj->log2Abs, obj->sumAbs, obj->numDopplerBins);
            }
            pingPongIdx ^= 1;
        }

        /* CFAR-detecton on current range line: search doppler peak among numDopplerBins samples */
        numDetObjPerCfar = mmwavelib_cfarCadBwrap(
                obj->sumAbs,
                obj->cfarDetObjIndexBuf,
                obj->numDopplerBins,
                obj->cfarCfgDoppler.thresholdScale,
                obj->cfarCfgDoppler.noiseDivShift,
                obj->cfarCfgDoppler.guardLen,
                obj->cfarCfgDoppler.winLen);


        if(numDetObjPerCfar > 0)
        {
            for (detIdx1 = 0; detIdx1 < numDetObjPerCfar; detIdx1++)
            {
                if (!MmwDemo_isSetDopplerLine(&obj->detDopplerLines, obj->cfarDetObjIndexBuf[detIdx1]))
                {
                    MmwDemo_setDopplerLine(&obj->detDopplerLines, obj->cfarDetObjIndexBuf[detIdx1]);
                    numDetObj1D++;
                }
            }
        }

        /* populate the pre-detection matrix */
        EDMA_startDmaTransfer(obj->edmaHandle[EDMA_INSTANCE_A], MMW_EDMA_CH_DET_MATRIX);
    }

    startTimeWait = Cycleprofiler_getTimeStamp();
    MmwDemo_dataPathWaitTransDetMatrix (obj);
    waitingTime += Cycleprofiler_getTimeStamp() - startTimeWait;

    /*Perform CFAR detection along range lines. Only those doppler bins which were
     * detected in the earlier CFAR along doppler dimension are considered
     */
    if (numDetObj1D > 0)
    {
        dopplerLine = MmwDemo_getDopplerLine(&obj->detDopplerLines);
        EDMAutil_triggerType3(obj->edmaHandle[EDMA_INSTANCE_A],
                (uint8_t *)(&obj->detMatrix[dopplerLine]),
                (uint8_t *)(SOC_translateAddress((uint32_t)(&obj->sumAbsRange[0]),
                                             SOC_TranslateAddr_Dir_TO_EDMA,NULL)),
                (uint8_t) MMW_EDMA_CH_DET_MATRIX2,
                (uint8_t) MMW_EDMA_TRIGGER_ENABLE);
    }

    numDetObj2D = 0;
    for (detIdx1 = 0; detIdx1 < numDetObj1D; detIdx1++)
    {
        /* wait for DMA transfer of current range line to complete */
        startTimeWait = Cycleprofiler_getTimeStamp();
        MmwDemo_dataPathWaitTransDetMatrix2 (obj);
        waitingTime += Cycleprofiler_getTimeStamp() - startTimeWait;

        /* Trigger next DMA */
        if(detIdx1 < (numDetObj1D -1))
        {
            dopplerLineNext = MmwDemo_getDopplerLine(&obj->detDopplerLines);

            EDMAutil_triggerType3(obj->edmaHandle[EDMA_INSTANCE_A],
                    (uint8_t*)(&obj->detMatrix[dopplerLineNext]),
                    (uint8_t*)(SOC_translateAddress(
                        (uint32_t)(&obj->sumAbsRange[((detIdx1 + 1) & 0x1) * obj->numRangeBins]),
                        SOC_TranslateAddr_Dir_TO_EDMA,NULL)),
                    (uint8_t) MMW_EDMA_CH_DET_MATRIX2,
                    (uint8_t) MMW_EDMA_TRIGGER_ENABLE);
        }
        /* On the detected doppler line, CFAR search the range peak among numRangeBins samples */
        numDetObjPerCfar = mmwavelib_cfarCadB_SOGO(
                &obj->sumAbsRange[(detIdx1 & 0x1) * obj->numRangeBins],
                obj->cfarDetObjIndexBuf,
                obj->numRangeBins,
                obj->cfarCfgRange.averageMode,
                obj->cfarCfgRange.thresholdScale,
                obj->cfarCfgRange.noiseDivShift,
                obj->cfarCfgRange.guardLen,
                obj->cfarCfgRange.winLen);

        if (numDetObjPerCfar > 0)
        {
            for(detIdx2=0; detIdx2 <numDetObjPerCfar; detIdx2++)
            {
                if (numDetObj2D < MAX_DET_OBJECTS_RAW)
                {
                    obj->detObj2DRaw[numDetObj2D].dopplerIdx = dopplerLine;
                    obj->detObj2DRaw[numDetObj2D].rangeIdx = obj->cfarDetObjIndexBuf[detIdx2];
                    obj->detObj2DRaw[numDetObj2D].peakVal = obj->sumAbsRange[(detIdx1 & 0x1) *
                        obj->numRangeBins + obj->cfarDetObjIndexBuf[detIdx2]];
                    numDetObj2D++;
                }
            }
        }
        dopplerLine = dopplerLineNext;
    }

    /* Peak grouping */
    obj->numDetObjRaw = numDetObj2D;
    if (obj->peakGroupingCfg.scheme == MMW_PEAK_GROUPING_CFAR_PEAK_BASED)
    {
        numDetObj2D = MmwDemo_cfarPeakGroupingCfarQualified( obj->detObj2D,
                                                obj->detObj2DRaw,
                                                numDetObj2D,
                                                obj->numDopplerBins,
                                                obj->peakGroupingCfg.maxRangeIndex,
                                                obj->peakGroupingCfg.minRangeIndex,
                                                obj->peakGroupingCfg.inDopplerDirectionEn,
                                                obj->peakGroupingCfg.inRangeDirectionEn);
    }
    else if (obj->peakGroupingCfg.scheme == MMW_PEAK_GROUPING_DET_MATRIX_BASED)
    {
        numDetObj2D = MmwDemo_cfarPeakGrouping( obj->detObj2D,
                                                obj->detObj2DRaw,
                                                numDetObj2D,
                                                obj->detMatrix,
                                                obj->numDopplerBins,
                                                obj->peakGroupingCfg.maxRangeIndex,
                                                obj->peakGroupingCfg.minRangeIndex,
                                                obj->peakGroupingCfg.inDopplerDirectionEn,
                                                obj->peakGroupingCfg.inRangeDirectionEn);
    }
    else
    {
        DebugP_assert(0);
    }
    obj->numDetObj = numDetObj2D;

    if (obj->numVirtualAntAzim > 1)
    {
        /**************************************
         *  Azimuth calculation
         **************************************/
        for (detIdx2 = 0; detIdx2 < numDetObj2D; detIdx2++)
        {
            uint32_t antIndx;

            /* Reset input buffer to azimuth FFT */
            memset((uint8_t *)obj->azimuthIn, 0, obj->numAngleBins * sizeof(cmplx32ReIm_t));

            /* Set source for first (ping) DMA and trigger it, and set source second (Pong) DMA */
            EDMAutil_triggerType3 (
                    obj->edmaHandle[EDMA_INSTANCE_A],
                    (uint8_t *)(&obj->radarCube[obj->numDopplerBins * obj->numRxAntennas *
                        obj->numTxAntennas * obj->detObj2D[detIdx2].rangeIdx]),
                    (uint8_t *) NULL,
                    (uint8_t) MMW_EDMA_CH_3D_IN_PING,
                    (uint8_t) MMW_EDMA_TRIGGER_ENABLE);
            EDMAutil_triggerType3 (
                    obj->edmaHandle[EDMA_INSTANCE_A],
                    (uint8_t *)(&obj->radarCube[(obj->numDopplerBins * obj->numRxAntennas *
                        obj->numTxAntennas * obj->detObj2D[detIdx2].rangeIdx + obj->numDopplerBins)]),
                    (uint8_t *) NULL,
                    (uint8_t) MMW_EDMA_CH_3D_IN_PONG,
                    (uint8_t) MMW_EDMA_TRIGGER_DISABLE);

            for (rxAntIdx = 0; rxAntIdx < (obj->numRxAntennas * obj->numTxAntennas); rxAntIdx++)
            {
                /* verify that previous DMA has completed */
                startTimeWait = Cycleprofiler_getTimeStamp();
                MmwDemo_dataPathWait3DInputData (obj, pingPongId(rxAntIdx));
                waitingTime += Cycleprofiler_getTimeStamp() - startTimeWait;

                /* kick off next DMA */
                if (rxAntIdx < (obj->numRxAntennas * obj->numTxAntennas) - 1)
                {
                    if (isPong(rxAntIdx))
                    {
                        EDMA_startDmaTransfer(obj->edmaHandle[EDMA_INSTANCE_A], MMW_EDMA_CH_3D_IN_PING);
                    }
                    else
                    {
                        EDMA_startDmaTransfer(obj->edmaHandle[EDMA_INSTANCE_A], MMW_EDMA_CH_3D_IN_PONG);
                    }
                }

                /* Calculate one bin DFT, at detected doppler index */
                mmwavelib_dftSingleBin(
                    (uint32_t *) &obj->dstPingPong[pingPongId(rxAntIdx) * obj->numDopplerBins],
                    (uint32_t *) obj->azimuthModCoefs,
                    (uint32_t *) &obj->azimuthIn[rxAntIdx],
                    obj->numDopplerBins,
                    obj->detObj2D[detIdx2].dopplerIdx);
            }

            /* Compensation of Doppler phase shift in the virtual antennas, (correponding
             * to second Tx antenna chirps). Symbols correponding to virtuall antennas,
             * are rotated by half of the Doppler phase shift measured by Doppler FFT.
             * The phase shift read from the table using half of the object
             * Doppler index  value. If the Doppler index is odd, an extra half
             * of the bin phase shift is added. */
            {
                cmplx16ImRe_t expDoppComp;
                uint32_t * pExpDoppComp = (uint32_t *) &expDoppComp;
                uint32_t * pAzimuthModCoefsHalfBin = (uint32_t *) &obj->azimuthModCoefsHalfBin;
                int32_t dopplerCompensationIdx;


                dopplerCompensationIdx = obj->detObj2D[detIdx2].dopplerIdx;
                if (dopplerCompensationIdx >= obj->numDopplerBins/2)
                {
                    dopplerCompensationIdx -=  (int32_t) obj->numDopplerBins;
                }
                dopplerCompensationIdx = dopplerCompensationIdx / 2;
                if (dopplerCompensationIdx < 0)
                {
                    dopplerCompensationIdx +=  (int32_t) obj->numDopplerBins;
                }


                expDoppComp = obj->azimuthModCoefs[dopplerCompensationIdx];
                if (obj->detObj2D[detIdx2].dopplerIdx & 0x1)
                {
                    /* Add half bin shift */
                    *pExpDoppComp = (uint32_t) _cmpyr1(*pExpDoppComp, *pAzimuthModCoefsHalfBin);
                }
                /*Compensate symbols on Rx antennas from the second chirp */
                {
#if 1
                    int64_t *azimuthIn = (int64_t *) obj->azimuthIn;
                    int64_t azimuthVal;
                    int32_t Re, Im;
                    uint32_t expDoppComp = *pExpDoppComp;
                    uint32_t numRxAnt = obj->numRxAntennas;
                    uint32_t numVirtAnt = obj->numRxAntennas * obj->numTxAntennas;
                    for (antIndx = numRxAnt; antIndx < numVirtAnt; antIndx++)
                    {
                        azimuthVal = _amem8(&azimuthIn[antIndx]);
                        Re = _ssub(_mpyhir(expDoppComp, _loll(azimuthVal) ),
                                    _mpylir(expDoppComp, _hill(azimuthVal)));
                        Im = _sadd(_mpylir(expDoppComp, _loll(azimuthVal)),
                                    _mpyhir(expDoppComp, _hill(azimuthVal)));
                        _amem8(&azimuthIn[antIndx]) =  _itoll(Im, Re);
                    }
#else
                    cmplx32ReIm_t temp;
                    for (antIndx = obj->numRxAntennas;
                         antIndx < (obj->numRxAntennas * obj->numTxAntennas); antIndx++)
                    {
                        temp.real = _ssub(_mpyhir((uint32_t) *pExpDoppComp, (int32_t) obj->azimuthIn[antIndx].real),
                                    _mpylir((uint32_t) *pExpDoppComp, (int32_t) obj->azimuthIn[antIndx].imag));
                        temp.imag = _sadd(_mpylir((uint32_t) *pExpDoppComp, (int32_t) obj->azimuthIn[antIndx].real),
                                    _mpyhir((uint32_t) *pExpDoppComp, (int32_t) obj->azimuthIn[antIndx].imag));

                        obj->azimuthIn[antIndx] = temp;
                    }
#endif
                }
            }
            memset((void *) &obj->azimuthIn[obj->numVirtualAntAzim], 0, (obj->numAngleBins - obj->numVirtualAntAzim) * sizeof(cmplx32ReIm_t));


            /* 3D-FFT (Azimuth FFT) */
            DSP_fft32x32(
                (int32_t *)obj->azimuthTwiddle32x32,
                obj->numAngleBins,
                (int32_t *) obj->azimuthIn,
                (int32_t *) obj->azimuthOut);

            MmwDemo_magnitudeSquared(
                obj->azimuthOut,
                obj->azimuthMagSqr,
                obj->numAngleBins);

            /* Calculate XY coordinates in meters */
            MmwDemo_XYestimation(obj, detIdx2);
        }

    }
    else
    {
        for (detIdx2 = 0; detIdx2 < numDetObj2D; detIdx2++)
        {
            /* Calculate Y coordinate in meters */
            MmwDemo_Yestimation(obj, detIdx2);
        }
    }
    gCycleLog.interFrameProcessingTime += Cycleprofiler_getTimeStamp() - startTime - waitingTime;
    gCycleLog.interFrameWaitTime += waitingTime;

}


/**
 *  @b Description
 *  @n
 *    Chirp processing. It is called from MmwDemo_dssDataPathProcessEvents. It
 *    is executed per chirp
 *
 *  @retval
 *      Not Applicable.
 */
 void MmwDemo_processChirp(MmwDemo_DSS_DataPathObj *obj)
{
    volatile uint32_t startTime;

    startTime = Cycleprofiler_getTimeStamp();
    if(obj->chirpCount > 1) //verify if ping(or pong) buffer is free for odd(or even) chirps
    {
        MmwDemo_dataPathWait1DOutputData (obj, pingPongId(obj->chirpCount));
    }
    gCycleLog.interChirpWaitTime += Cycleprofiler_getTimeStamp() - startTime;

    MmwDemo_interChirpProcessing (obj, pingPongId(obj->chirpCount));

    /*Modify destination address in Param set and DMA for sending 1DFFT output (for all antennas) to L3  */
    if(isPong(obj->chirpCount))
    {
        EDMAutil_triggerType3 (
            obj->edmaHandle[EDMA_INSTANCE_A],
            (uint8_t *) NULL,
            (uint8_t *)(&obj->radarCube[(obj->numDopplerBins * obj->numRxAntennas *
                (obj->numTxAntennas-1)) + obj->dopplerBinCount]),
            (uint8_t) MMW_EDMA_CH_1D_OUT_PONG,
            (uint8_t) MMW_EDMA_TRIGGER_ENABLE);
    }
    else
    {
        EDMAutil_triggerType3 (
            obj->edmaHandle[EDMA_INSTANCE_A],
            (uint8_t *) NULL,
            (uint8_t *)(&obj->radarCube[obj->dopplerBinCount]),
            (uint8_t) MMW_EDMA_CH_1D_OUT_PING,
            (uint8_t) MMW_EDMA_TRIGGER_ENABLE);
    }
    obj->chirpCount++;
    obj->txAntennaCount++;
    if(obj->txAntennaCount == obj->numTxAntennas)
    {
        obj->txAntennaCount = 0;
        obj->dopplerBinCount++;
        if (obj->dopplerBinCount == obj->numDopplerBins)
        {
            obj->dopplerBinCount = 0;
            obj->chirpCount = 0;
        }
    }
}

 /**
  *  @b Description
  *  @n
  *  Wait for transfer of data corresponding to the last 2 chirps (ping/pong)
  *  to the radarCube matrix before starting interframe processing.
  *  @retval
  *      Not Applicable.
  */
void MmwDemo_waitEndOfChirps(MmwDemo_DSS_DataPathObj *obj)
{
    volatile uint32_t startTime;

    startTime = Cycleprofiler_getTimeStamp();
    /* Wait for transfer of data corresponding to last 2 chirps (ping/pong) */
    MmwDemo_dataPathWait1DOutputData (obj, 0);
    MmwDemo_dataPathWait1DOutputData (obj, 1);

    gCycleLog.interChirpWaitTime += Cycleprofiler_getTimeStamp() - startTime;
}

/**
 *  @b Description
 *  @n
 *  Generate SIN/COS table in Q15 (SIN to even int16 location, COS to
 *  odd int16 location. Also generates Sin/Cos at half the bin value
 *  The table is generated as
 *  T[i]=cos[2*pi*i/N] - 1j*sin[2*pi*i/N] for i=0,...,N where N is dftLen
 *  The half bn value is calculated as:
 *  TH = cos[2*pi*0.5/N] - 1j*sin[2*pi*0.5/N]
 *
 *  @param[out]    dftSinCosTable Array with generated Sin Cos table
 *  @param[out]    dftHalfBinVal  Sin/Cos value at half the bin
 *  @param[in]     dftLen Length of the DFT
 *
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_genDftSinCosTable(cmplx16ImRe_t *dftSinCosTable,
                            cmplx16ImRe_t *dftHalfBinVal,
                            uint32_t dftLen)
{
    uint32_t i;
    int32_t itemp;
    float temp;
    for (i = 0; i < dftLen; i++)
    {
        temp = ONE_Q15 * -sin(2*PI_*i/dftLen);
        itemp = (int32_t) ROUND(temp);

        if(itemp >= ONE_Q15)
        {
            itemp = ONE_Q15 - 1;
        }
        dftSinCosTable[i].imag = itemp;

        temp = ONE_Q15 * cos(2*PI_*i/dftLen);
        itemp = (int32_t) ROUND(temp);

        if(itemp >= ONE_Q15)
        {
            itemp = ONE_Q15 - 1;
        }
        dftSinCosTable[i].real = itemp;
    }

    /*Calculate half bin value*/
    temp = ONE_Q15 * - sin(PI_/dftLen);
    itemp = (int32_t) ROUND(temp);

    if(itemp >= ONE_Q15)
    {
        itemp = ONE_Q15 - 1;
    }
    dftHalfBinVal[0].imag = itemp;

    temp = ONE_Q15 * cos(PI_/dftLen);
    itemp = (int32_t) ROUND(temp);

    if(itemp >= ONE_Q15)
    {
        itemp = ONE_Q15 - 1;
    }
    dftHalfBinVal[0].real = itemp;
}

void MmwDemo_edmaErrorCallbackFxn(EDMA_Handle handle, EDMA_errorInfo_t *errorInfo)
{
    DebugP_assert(0);
}

void MmwDemo_edmaTransferControllerErrorCallbackFxn(EDMA_Handle handle,
                EDMA_transferControllerErrorInfo_t *errorInfo)
{
    DebugP_assert(0);
}

void MmwDemo_dataPathInit1Dstate(MmwDemo_DSS_DataPathObj *obj)
{
    obj->chirpCount = 0;
    obj->dopplerBinCount = 0;
    obj->txAntennaCount = 0;

    /* reset profiling logs before start of frame */
    memset((void *) &gCycleLog, 0, sizeof(cycleLog_t));
}

void MmwDemo_dataPathDeleteSemaphore(MmwDemo_DSS_DataPathObj *obj)
{
    Semaphore_delete(&obj->EDMA_1D_InputDone_semHandle[0]);
    Semaphore_delete(&obj->EDMA_1D_InputDone_semHandle[1]);
    Semaphore_delete(&obj->EDMA_1D_OutputDone_semHandle[0]);
    Semaphore_delete(&obj->EDMA_1D_OutputDone_semHandle[1]);
    Semaphore_delete(&obj->EDMA_2D_InputDone_semHandle[0]);
    Semaphore_delete(&obj->EDMA_2D_InputDone_semHandle[1]);
    Semaphore_delete(&obj->EDMA_DetMatrix_semHandle);
    Semaphore_delete(&obj->EDMA_DetMatrix2_semHandle);
    Semaphore_delete(&obj->EDMA_3D_InputDone_semHandle[0]);
    Semaphore_delete(&obj->EDMA_3D_InputDone_semHandle[1]);
}

int32_t MmwDemo_dataPathInitEdma(MmwDemo_DSS_DataPathObj *obj)
{
    Semaphore_Params       semParams;
    uint8_t numInstances;
    int32_t errorCode;
    EDMA_Handle handle;
    EDMA_errorConfig_t errorConfig;
    uint32_t instanceId;
    EDMA_instanceInfo_t instanceInfo;

    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    obj->EDMA_1D_InputDone_semHandle[0] = Semaphore_create(0, &semParams, NULL);
    obj->EDMA_1D_InputDone_semHandle[1] = Semaphore_create(0, &semParams, NULL);
    obj->EDMA_1D_OutputDone_semHandle[0] = Semaphore_create(0, &semParams, NULL);
    obj->EDMA_1D_OutputDone_semHandle[1] = Semaphore_create(0, &semParams, NULL);
    obj->EDMA_2D_InputDone_semHandle[0] = Semaphore_create(0, &semParams, NULL);
    obj->EDMA_2D_InputDone_semHandle[1] = Semaphore_create(0, &semParams, NULL);
    obj->EDMA_DetMatrix_semHandle = Semaphore_create(0, &semParams, NULL);
    obj->EDMA_DetMatrix2_semHandle = Semaphore_create(0, &semParams, NULL);
    obj->EDMA_3D_InputDone_semHandle[0] = Semaphore_create(0, &semParams, NULL);
    obj->EDMA_3D_InputDone_semHandle[1] = Semaphore_create(0, &semParams, NULL);

    numInstances = EDMA_getNumInstances();

    /* Initialize the edma instance to be tested */
    for(instanceId = 0; instanceId < numInstances; instanceId++) {
        EDMA_init(instanceId);

        handle = EDMA_open(instanceId, &errorCode, &instanceInfo);
        if (handle == NULL)
        {
            System_printf("Error: Unable to open the edma Instance, erorCode = %d\n", errorCode);
            return -1;
        }
        obj->edmaHandle[instanceId] = handle;

        errorConfig.isConfigAllEventQueues = true;
        errorConfig.isConfigAllTransferControllers = true;
        errorConfig.isEventQueueThresholdingEnabled = true;
        errorConfig.eventQueueThreshold = EDMA_EVENT_QUEUE_THRESHOLD_MAX;
        errorConfig.isEnableAllTransferControllerErrors = true;
        errorConfig.callbackFxn = MmwDemo_edmaErrorCallbackFxn;
        errorConfig.transferControllerCallbackFxn = MmwDemo_edmaTransferControllerErrorCallbackFxn;
        if ((errorCode = EDMA_configErrorMonitoring(handle, &errorConfig)) != EDMA_NO_ERROR)
        {
            System_printf("Debug: EDMA_configErrorMonitoring() failed with errorCode = %d\n", errorCode);
            return -1;
        }
    }
    return 0;
}

void MmwDemo_printHeapStats(char *name, uint32_t heapUsed, uint32_t heapSize)
{ 
    System_printf("Heap %s : size %d (0x%x), free %d (0x%x)\n", name, heapSize, heapSize,
        heapSize - heapUsed, heapSize - heapUsed);
}


#define SOC_MAX_NUM_RX_ANTENNAS 4
#define SOC_MAX_NUM_TX_ANTENNAS 2

void MmwDemo_dataPathConfigAzimuthHeatMap(MmwDemo_DSS_DataPathObj *obj)
{
    obj->log2NumDopplerBins = (uint32_t) log2sp(obj->numDopplerBins);
}

void MmwDemo_dataPathConfigBuffers(MmwDemo_DSS_DataPathObj *obj, uint32_t adcBufAddress)
{
/* below defines for debugging purposes, do not remove as overlays can be hard to debug */
//#define NO_L1_ALLOC /* don't allocate from L1D, use L2 instead */
//#define NO_OVERLAY  /* do not overlay */

#define ALIGN(x,a)  (((x)+((a)-1))&~((a)-1))

#ifdef NO_OVERLAY
#define MMW_ALLOC_BUF(name, nameType, startAddr, alignment, size) \
        obj->name = (nameType *) ALIGN(prev_end, alignment); \
        prev_end = (uint32_t)obj->name + (size) * sizeof(nameType);
#else
#define MMW_ALLOC_BUF(name, nameType, startAddr, alignment, size) \
        obj->name = (nameType *) ALIGN(startAddr, alignment); \
        uint32_t name##_end = (uint32_t)obj->name + (size) * sizeof(nameType);
#endif

    uint32_t heapUsed;
    uint32_t heapL1start = (uint32_t) &gMmwL1[0];
    uint32_t heapL2start = (uint32_t) &gMmwL2[0];
    uint32_t heapL3start = (uint32_t) &gMmwL3[0];

    /* L3 is overlaid with one-time only accessed code. Although heap is not
       required to be initialized to 0, it may help during debugging when viewing memory
       in CCS */
    memset((void *)heapL3start, 0, SOC_XWR16XX_DSS_L3RAM_SIZE);

    /* L1 allocation
       
        Buffers are overlayed in the following order. Notation "|" indicates parallel
        and "+" means cascade 

        { 1D
            (adcDataIn)
        } |
        { 2D
           (dstPingPong +  fftOut2D) +
           (windowingBuf2D | log2Abs) + sumAbs
        } |
        { 3D
           (detObjRaw +
            azimuthIn (must be at least beyond dstPingPong) + azimuthOut + azimuthMagSqr)
        }
    */
#ifdef NO_L1_ALLOC
    heapL1start = heapL2start;
#endif
#ifdef NO_OVERLAY
    uint32_t prev_end = heapL1start;
#endif

    MMW_ALLOC_BUF(adcDataIn, cmplx16ReIm_t, 
        heapL1start, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN,
        2 * obj->numRangeBins);
    memset((void *)obj->adcDataIn, 0, 2 * obj->numRangeBins * sizeof(cmplx16ReIm_t));

    MMW_ALLOC_BUF(dstPingPong, cmplx16ReIm_t, 
        heapL1start, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN, 
        2 * obj->numDopplerBins);

    MMW_ALLOC_BUF(fftOut2D, cmplx32ReIm_t, 
        dstPingPong_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN, 
        obj->numDopplerBins); 
   
    MMW_ALLOC_BUF(windowingBuf2D, cmplx32ReIm_t, 
        fftOut2D_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN, 
        obj->numDopplerBins);     

    MMW_ALLOC_BUF(log2Abs, uint16_t, 
        fftOut2D_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN, 
        obj->numDopplerBins); 

    MMW_ALLOC_BUF(sumAbs, uint16_t, 
        MAX(log2Abs_end, windowingBuf2D_end), MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN, 
        2 * obj->numDopplerBins); 

    MMW_ALLOC_BUF(detObj2DRaw, MmwDemo_objRaw_t, 
        heapL1start, MMWDEMO_MEMORY_ALLOC_MAX_STRUCT_ALIGN, 
        MAX_DET_OBJECTS_RAW); 
        
    MMW_ALLOC_BUF(azimuthIn, cmplx32ReIm_t, 
        MAX(detObj2DRaw_end, dstPingPong_end), MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN, 
        obj->numAngleBins);         

    MMW_ALLOC_BUF(azimuthOut, cmplx32ReIm_t, 
        azimuthIn_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN, 
        obj->numAngleBins);  

    MMW_ALLOC_BUF(azimuthMagSqr, float, 
        azimuthOut_end, sizeof(float), 
        obj->numAngleBins);
        
#ifndef NO_L1_ALLOC 
#ifdef NO_OVERLAY
    heapUsed = prev_end  - heapL1start;
#else       
    heapUsed = MAX(MAX(sumAbs_end, adcDataIn_end), 
                     azimuthMagSqr_end) - heapL1start;
#endif
    DebugP_assert(heapUsed <= MMW_L1_HEAP_SIZE);
    MmwDemo_printHeapStats("L1", heapUsed, MMW_L1_HEAP_SIZE);
#endif
    /* L2 allocation
        {
            { 1D
                (fftOut1D)
            } |
            { 2D + 3D
               (cfarDetObjIndexBuf + detDopplerLines.dopplerLineMask) + sumAbsRange
            }
        } +
        {
            twiddle16x16_1D +
            window1D +
            twiddle32x32_2D +
            window2D +
            detObj2D +
            detObj2dAzimIdx +
            azimuthTwiddle32x32 +
            azimuthModCoefs      
        }
    */
#ifdef NO_L1_ALLOC
#ifdef NO_OVERLAY
    heapL2start = prev_end;
#else
    heapL2start = MAX(MAX(sumAbs_end, adcDataIn_end), azimuthMagSqr_end);
#endif
#else
#ifdef NO_OVERLAY
    prev_end = heapL2start;
#endif
#endif

    MMW_ALLOC_BUF(fftOut1D, cmplx16ReIm_t, 
        heapL2start, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN,
        2 * obj->numRxAntennas * obj->numRangeBins); 
       
    MMW_ALLOC_BUF(cfarDetObjIndexBuf, uint16_t, 
        heapL2start, sizeof(uint16_t), 
        MAX(obj->numRangeBins, obj->numDopplerBins)); 
    
    /* Expansion of below macro (macro cannot be used due to x.y type reference)
        MMW_ALLOC_BUF(detDopplerLines.dopplerLineMask, uint32_t, 
            cfarDetObjIndexBuf_end, MMWDEMO_MEMORY_ALLOC_MAX_STRUCT_ALIGN, 
            MAX((obj->numDopplerBins>>5),1));
    */
#ifdef NO_OVERLAY
    obj->detDopplerLines.dopplerLineMask = (uint32_t *) ALIGN(prev_end, 
        MMWDEMO_MEMORY_ALLOC_MAX_STRUCT_ALIGN);
    prev_end = (uint32_t)obj->detDopplerLines.dopplerLineMask  + 
                MAX((obj->numDopplerBins>>5),1) * sizeof(uint32_t);
#else
    obj->detDopplerLines.dopplerLineMask = (uint32_t *) ALIGN(cfarDetObjIndexBuf_end, 
        MMWDEMO_MEMORY_ALLOC_MAX_STRUCT_ALIGN);
    uint32_t detDopplerLines_dopplerLineMask_end = (uint32_t)obj->detDopplerLines.dopplerLineMask  + 
                MAX((obj->numDopplerBins>>5),1) * sizeof(uint32_t);
#endif

    obj->detDopplerLines.dopplerLineMaskLen = MAX((obj->numDopplerBins>>5),1);
    
    MMW_ALLOC_BUF(sumAbsRange, uint16_t, 
        detDopplerLines_dopplerLineMask_end, sizeof(uint16_t),
        2 * obj->numRangeBins);
       
    MMW_ALLOC_BUF(twiddle16x16_1D, cmplx16ReIm_t, 
        MAX(fftOut1D_end, sumAbsRange_end), 
        MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN,
        obj->numRangeBins);
        
    MMW_ALLOC_BUF(window1D, int16_t, 
        twiddle16x16_1D_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN,
        obj->numAdcSamples / 2);

    MMW_ALLOC_BUF(twiddle32x32_2D, cmplx32ReIm_t, 
        window1D_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN,
        obj->numDopplerBins);

    MMW_ALLOC_BUF(window2D, int32_t, 
        twiddle32x32_2D_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN,
        obj->numDopplerBins / 2);        

    MMW_ALLOC_BUF(detObj2D, MmwDemo_detectedObj, 
        window2D_end, MMWDEMO_MEMORY_ALLOC_MAX_STRUCT_ALIGN,
        MMW_MAX_OBJ_OUT);

    MMW_ALLOC_BUF(detObj2dAzimIdx, uint8_t, 
        detObj2D_end, MMWDEMO_MEMORY_ALLOC_MAX_STRUCT_ALIGN,
        MMW_MAX_OBJ_OUT);
        
    MMW_ALLOC_BUF(azimuthTwiddle32x32, cmplx32ReIm_t, 
        detObj2dAzimIdx_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN,
        obj->numAngleBins); 

    MMW_ALLOC_BUF(azimuthModCoefs, cmplx16ImRe_t, 
        azimuthTwiddle32x32_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN,
        obj->numDopplerBins);

    MMW_ALLOC_BUF(dcRangeSigMean, cmplx32ImRe_t,
        azimuthModCoefs_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN,
        SOC_MAX_NUM_TX_ANTENNAS * SOC_MAX_NUM_RX_ANTENNAS * DC_RANGE_SIGNATURE_COMP_MAX_BIN_SIZE);

#ifdef NO_OVERLAY
    heapUsed = prev_end - heapL2start;
#else        
    heapUsed = dcRangeSigMean_end - heapL2start;
#endif
    DebugP_assert(heapUsed <= MMW_L2_HEAP_SIZE);
    MmwDemo_printHeapStats("L2", heapUsed, MMW_L2_HEAP_SIZE);    

    /* L3 allocation:
        ADCdataBuf (for unit test) +
        radarCube +
        azimuthStaticHeatMap +
        detMatrix
    */
#ifdef NO_OVERLAY
    prev_end = heapL3start;
#endif    
    MMW_ALLOC_BUF(ADCdataBuf, cmplx16ReIm_t, 
        heapL3start, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN, obj->numRangeBins * obj->numRxAntennas * obj->numTxAntennas);
    
    if (adcBufAddress != NULL)
    {
        obj->ADCdataBuf = (cmplx16ReIm_t *)adcBufAddress;
#ifndef NO_OVERLAY
        ADCdataBuf_end = heapL3start;
#endif
    }
    
    MMW_ALLOC_BUF(radarCube, cmplx16ReIm_t, 
        ADCdataBuf_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN, 
        obj->numRangeBins * obj->numDopplerBins * obj->numRxAntennas *
        obj->numTxAntennas);

    MMW_ALLOC_BUF(azimuthStaticHeatMap, cmplx16ImRe_t, 
        radarCube_end, MMWDEMO_MEMORY_ALLOC_DOUBLE_WORD_ALIGN, 
        obj->numRangeBins * obj->numRxAntennas * obj->numTxAntennas);
   
    MMW_ALLOC_BUF(detMatrix, uint16_t, 
        azimuthStaticHeatMap_end, sizeof(uint16_t), 
        obj->numRangeBins * obj->numDopplerBins);

#ifdef NO_OVERLAY
    heapUsed = prev_end - heapL3start;
#else
    heapUsed = detMatrix_end - heapL3start;
#endif
    DebugP_assert(heapUsed <= SOC_XWR16XX_DSS_L3RAM_SIZE);
    MmwDemo_printHeapStats("L3", heapUsed, SOC_XWR16XX_DSS_L3RAM_SIZE);
}

void MmwDemo_dataPathConfigFFTs(MmwDemo_DSS_DataPathObj *obj)
{
    MmwDemo_genWindow((void *)obj->window1D,
                        FFT_WINDOW_INT16,
                        obj->numAdcSamples,
                        obj->numAdcSamples/2,
                        ONE_Q15,
                        MMW_WIN_BLACKMAN);

    MmwDemo_genWindow((void *)obj->window2D,
                        FFT_WINDOW_INT32,
                        obj->numDopplerBins,
                        obj->numDopplerBins/2,
                        ONE_Q19,
                        MMW_WIN_HANNING);

    /* Generate twiddle factors for 1D FFT. This is one time */
    gen_twiddle_fft16x16((int16_t *)obj->twiddle16x16_1D, obj->numRangeBins);

    /* Generate twiddle factors for 2D FFT */
    gen_twiddle_fft32x32((int32_t *)obj->twiddle32x32_2D, obj->numDopplerBins, 2147483647.5);

    /* Generate twiddle factors for azimuth FFT */
    gen_twiddle_fft32x32((int32_t *)obj->azimuthTwiddle32x32, obj->numAngleBins, 2147483647.5);

    /* Generate SIN/COS table for single point DFT */
    MmwDemo_genDftSinCosTable(obj->azimuthModCoefs,
                              &obj->azimuthModCoefsHalfBin,
                              obj->numDopplerBins);

}

/**
 *  @b Description
 *  @n
 *      Function to generate a single FFT window sample.
 *
 *  @param[out] win             Pointer to calculated window samples.
 *  @param[in]  windowDatumType Window samples data format. For windowDatumType = @ref FFT_WINDOW_INT16,
 *              the samples format is int16_t. For windowDatumType = @ref FFT_WINDOW_INT32,
 *              the samples format is int32_t.
 *  @param[in]  winLen          Nominal window length
 *  @param[in]  winGenLen       Number of generated samples
 *  @param[in]  oneQformat      Q format of samples, oneQformat is the value of
 *                              one in the desired format.
 *  @param[in]  winType         Type of window, one of @ref MMW_WIN_BLACKMAN, @ref MMW_WIN_HANNING,
 *              or @ref MMW_WIN_RECT.
 *  @retval none.
 */
void MmwDemo_genWindow(void *win,
                        uint32_t windowDatumType,
                        uint32_t winLen,
                        uint32_t winGenLen,
                        int32_t oneQformat,
                        uint32_t winType)
{
    uint32_t winIndx;
    int32_t winVal;
    int16_t * win16 = (int16_t *) win;
    int32_t * win32 = (int32_t *) win;

    float phi = 2 * PI_ / ((float) winLen - 1);

    if(winType == MMW_WIN_BLACKMAN)
    {
        //Blackman window
        float a0 = 0.42;
        float a1 = 0.5;
        float a2 = 0.08;
        for(winIndx = 0; winIndx < winGenLen; winIndx++)
        {
            winVal = (int32_t) ((oneQformat * (a0 - a1*cos(phi * winIndx) +
                a2*cos(2 * phi * winIndx))) + 0.5);
            if(winVal >= oneQformat)
            {
                winVal = oneQformat - 1;
            }
            switch (windowDatumType)
            {
                case FFT_WINDOW_INT16:
                    win16[winIndx] = (int16_t) winVal;
                    break;
                case FFT_WINDOW_INT32:
                    win32[winIndx] = (int32_t) winVal;
                    break;
             }

        }
    }
    else if (winType == MMW_WIN_HANNING)
    {
        //Hanning window
        for(winIndx = 0; winIndx < winGenLen; winIndx++)
        {
            winVal = (int32_t) ((oneQformat * 0.5* (1 - cos(phi * winIndx))) + 0.5);
            if(winVal >= oneQformat)
            {
                winVal = oneQformat - 1;
            }
            switch (windowDatumType)
            {
                case FFT_WINDOW_INT16:
                    win16[winIndx] = (int16_t) winVal;
                    break;
                case FFT_WINDOW_INT32:
                    win32[winIndx] = (int32_t) winVal;
                    break;
            }
        }
    }
    else if (winType == MMW_WIN_RECT)
    {
        //Rectangular window
        for(winIndx = 0; winIndx < winGenLen; winIndx++)
        {
            switch (windowDatumType)
            {
                case FFT_WINDOW_INT16:
                    win16[winIndx] =  (int16_t)  (oneQformat-1);
                    break;
                case FFT_WINDOW_INT32:
                    win32[winIndx] = (int32_t) (oneQformat-1);
                    break;
            }
        }
    }
}


