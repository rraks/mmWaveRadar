/*
 *   @file  dss_main.c
 *
 *   @brief
 *      Millimeter Wave Demo running on DSS
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
#include <ti/sysbios/family/c64p/Cache.h>
#include <ti/sysbios/family/c64p/Hwi.h>
#include <ti/sysbios/family/c64p/EventCombiner.h>
#include <ti/sysbios/utils/Load.h>


/* MMWSDK Include Files. */
#include <ti/drivers/soc/soc.h>
#include <ti/common/sys_common.h>
#include <ti/common/mmwave_sdk_version.h>
#include <ti/utils/cycleprofiler/cycle_profiler.h>

/* MMWAVE Demo Include Files */
#include <ti/demo/xwr16xx/mmw/dss/dss_mmw.h>
#include <ti/demo/xwr16xx/mmw/dss/dss_data_path.h>
#include <ti/demo/xwr16xx/mmw/common/mmw_messages.h>

/* C674x mathlib */
#include <ti/mathlib/mathlib.h>

/* Related to linker copy table for copying from L3 to L1PSRAM for example */
#include <cpy_tbl.h>

/**************************************************************************
 *************************** MmwDemo External DSS Functions ******************
 **************************************************************************/


/**************************************************************************
 *************************** Global Definitions ********************************
 **************************************************************************/
/*! DSS stores demo output in HSRAM  */
#pragma DATA_SECTION(gHSRam, ".demoSharedMem");
#pragma DATA_ALIGN(gHSRam, 4);
void *gHSRam;

/*! @brief Azimuth FFT size */
#define MMW_NUM_ANGLE_BINS 64

/*! @brief Flag to enable/disable two peak detection in azimuth for same range and velocity */
#define MMWDEMO_AZIMUTH_TWO_PEAK_DETECTION_ENABLE 1

/*! @brief Threshold for two peak detection in azimuth for same range and velocity,
 *         if 2nd peak heigth > first peak height * this scale then declare
 *         2nd peak as detected. The peaks are in @ref MmwDemo_DSS_DataPathObj::azimuthMagSqr */
#define MMWDEMO_AZIMUTH_TWO_PEAK_THRESHOLD_SCALE  (0.5)


#define MMWDEMO_SPEED_OF_LIGHT_IN_METERS_PER_SEC (3.0e8)

//#define DBG

/**
 * @brief
 *  Global Variable for tracking information required by the mmw Demo
 */
MmwDemo_DSS_MCB    gMmwDssMCB;

volatile cycleLog_t gCycleLog;

/* copy table related */
extern far COPY_TABLE _MmwDemo_fastCode_L1PSRAM_copy_table;

/**************************************************************************
 ************************* MmwDemo Functions Prototype  **********************
 **************************************************************************/

/* Copy table related */
static void MmwDemo_edmaBlockCopy(EDMA_Handle handle, uint32_t loadAddr, uint32_t runAddr, uint16_t size);
static void MmwDemo_copyTable(EDMA_Handle handle, COPY_TABLE *tp);

/* Internal DataPath Functions */
int32_t MmwDemo_dssDataPathInit(void);
static int32_t MmwDemo_dssDataPathConfig(void);
static int32_t MmwDemo_dssDataPathStart(bool doRFStart);
static int32_t MmwDemo_dssDataPathStop(void);
static int32_t MmwDemo_dssDataPathProcessEvents(UInt event);

/* Internal MMWave Call back Functions */
static void MmwDemo_dssMmwaveEventCallbackFxn(uint16_t msgId, uint16_t sbId, uint16_t sbLen, uint8_t *payload);
static void MmwDemo_dssMmwaveConfigCallbackFxn(MMWave_CtrlCfg* ptrCtrlCfg);
static void MmwDemo_dssMmwaveStartCallbackFxn(void);
static void MmwDemo_dssMmwaveStopCallbackFxn(void);


/* Internal Interrupt handler */
static void MmwDemo_dssChirpIntHandler(uintptr_t arg);
static void MmwDemo_dssFrameStartIntHandler(uintptr_t arg);

/* Internal mmwDemo Tasks running on DSS */
static void MmwDemo_dssInitTask(UArg arg0, UArg arg1);
static void MmwDemo_dssDataPathTask(UArg arg0, UArg arg1);
static void MmwDemo_dssMMWaveCtrlTask(UArg arg0, UArg arg1);

static int32_t MmwDemo_dssSendProcessOutputToMSS
(
    uint8_t           *ptrHsmBuffer,
    uint32_t           outputBufSize,
    MmwDemo_DSS_DataPathObj   *obj
);
void MmwDemo_dssDataPathOutputLogging(    MmwDemo_DSS_DataPathObj   * dataPathObj);

/**************************************************************************
 *************************** MmwDemo DSS Functions **************************
 **************************************************************************/

/**
 *  @b Description
 *  @n
 *      Interrupt handler callback for chirp available. It runs in the ISR context.
 *
 *  @retval
 *      Not Applicable.
 */
 #ifdef DBG
 #define NUM_CHIRP_TIME_STAMPS 128
 uint32_t gChirpTimeStamp[NUM_CHIRP_TIME_STAMPS];
 #endif

static void MmwDemo_dssChirpIntHandler(uintptr_t arg)
{
    MmwDemo_DSS_DataPathObj * dpObj = &gMmwDssMCB.dataPathObj;

    if((gMmwDssMCB.state == MmwDemo_DSS_STATE_STOPPED) ||
       (gMmwDssMCB.dataPathObj.interFrameProcToken<=0))
    {
        gMmwDssMCB.stats.chirpIntSkipCounter++;
        return;
    }

#ifdef DBG
    if (dpObj->chirpCount < NUM_CHIRP_TIME_STAMPS)
    {
        gChirpTimeStamp[dpObj->chirpCount] =
            Cycleprofiler_getTimeStamp();
    }
#endif

    if (dpObj->chirpCount == 0)
    {
        /* Note: this is valid after the first frame */
        dpObj->timingInfo.interFrameProcessingEndMargin =
            Cycleprofiler_getTimeStamp() - dpObj->timingInfo.interFrameProcessingEndTime;
    }
    else if (dpObj->chirpCount == 1)
    {
        dpObj->timingInfo.chirpProcessingEndMarginMin =
            Cycleprofiler_getTimeStamp() - dpObj->timingInfo.chirpProcessingEndTime;
        dpObj->timingInfo.chirpProcessingEndMarginMax =
            dpObj->timingInfo.chirpProcessingEndMarginMin;
    }
    else
    {
        uint32_t margin = Cycleprofiler_getTimeStamp() - dpObj->timingInfo.chirpProcessingEndTime;
        if (margin > dpObj->timingInfo.chirpProcessingEndMarginMax)
        {
            dpObj->timingInfo.chirpProcessingEndMarginMax = margin;
        }
        if (margin < dpObj->timingInfo.chirpProcessingEndMarginMin)
        {
            dpObj->timingInfo.chirpProcessingEndMarginMin = margin;
        }
    }
    /* Increment interrupt counter for debugging purpose */
    gMmwDssMCB.stats.chirpIntCounter++;

    /* Check if previous chirp processing has completed */
    DebugP_assert(dpObj->chirpProcToken == 0);
    dpObj->chirpProcToken++;

    /* Post event to notify chirp available interrupt */
    Event_post(gMmwDssMCB.eventHandle, MMWDEMO_CHIRP_EVT);
}

/**
 *  @b Description
 *  @n
 *      Interrupt handler callback for frame start ISR.
 *
 *  @retval
 *      Not Applicable.
 */
static void MmwDemo_dssFrameStartIntHandler(uintptr_t arg)
{
    if(gMmwDssMCB.state == MmwDemo_DSS_STATE_STOPPED)
    {
        gMmwDssMCB.stats.frameIntSkipCounter++;
        return;
    }

    if(gMmwDssMCB.state == MmwDemo_DSS_STATE_STOP_PENDING)
    {
        /* stop the clock as the DSP will be stopped at the end of this active frame */
        Clock_stop(gMmwDssMCB.frameClkHandle);
    }

    /* Check if previous chirp processing has completed */
    DebugP_assert(gMmwDssMCB.dataPathObj.interFrameProcToken == 0);
    gMmwDssMCB.dataPathObj.interFrameProcToken++;

    /* Increment interrupt counter for debugging purpose */
    gMmwDssMCB.stats.frameStartIntCounter++;

    /* Post event to notify frame start interrupt */
    Event_post(gMmwDssMCB.eventHandle, MMWDEMO_FRAMESTART_EVT);
}

/**
 *  @b Description
 *  @n
 *      Registered event callback function on DSS which is invoked by MMWAVE library when an event from the
 *      BSS is received.
 *
 *  @param[in]  msgId
 *      Message Identifier
 *  @param[in]  sbId
 *      Subblock identifier
 *  @param[in]  sbLen
 *      Length of the subblock
 *  @param[in]  payload
 *      Pointer to the payload buffer
 *
 *  @retval
 *      Not applicable
 */
static void MmwDemo_dssMmwaveEventCallbackFxn(uint16_t msgId, uint16_t sbId, uint16_t sbLen, uint8_t *payload)
{
    uint16_t asyncSB = RL_GET_SBID_FROM_UNIQ_SBID(sbId);

    /* Debug Message: */
    /*System_printf ("Debug: MMWDemoDSS received BSS Event MsgId: %d [Sub Block Id: %d Sub Block Length: %d]\n",
                    msgId, sbId, sbLen); */

    /* Process the received message: */
    switch (msgId)
    {
        case RL_RF_ASYNC_EVENT_MSG:
        {
            /* Received Asychronous Message: */
            switch (asyncSB)
            {
                case RL_RF_AE_CPUFAULT_SB:
                {
                    /* BSS fault */
                    DebugP_assert(0);
                    break;
                }
                case RL_RF_AE_ESMFAULT_SB:
                {
                    /* BSS fault */
                    DebugP_assert(0);
                    break;
                }
                case RL_RF_AE_INITCALIBSTATUS_SB:
                {
                    /* This event should be handled by mmwave internally, ignore the event here */
                    break;
                }

                case RL_RF_AE_FRAME_TRIGGER_RDY_SB:
                {
                    /* Post event to datapath task notify BSS events */
                    Event_post(gMmwDssMCB.eventHandle, MMWDEMO_BSS_FRAME_TRIGGER_READY_EVT);

                    break;
                }
                case RL_RF_AE_MON_TIMING_FAIL_REPORT_SB:
                {
                    /* Increment the statistics to reports that the calibration failed */
                    gMmwDssMCB.stats.numFailedTimingReports++;
                    break;
                }
                case RL_RF_AE_RUN_TIME_CALIB_REPORT_SB:
                {
                    /* Increment the statistics to indicate that a calibration report was received */
                    gMmwDssMCB.stats.numCalibrationReports++;
                    break;
                }
                default:
                {
                    System_printf ("Error: Asynchronous Event SB Id %d not handled\n", asyncSB);
                    break;
                }
            }
            break;
        }
        default:
        {
            System_printf ("Error: Asynchronous message %d is NOT handled\n", msgId);
            break;
        }
    }

    return;
}

/**
 *  @b Description
 *  @n
 *      Registered config callback function on DSS which is invoked by MMWAVE library when the remote side
 *  has finished configure mmWaveLink and BSS. The configuration need to be saved on DSS and used for DataPath.
 *
 *  @param[in]  ptrCtrlCfg
 *      Pointer to the control configuration
 *
 *  @retval
 *      Not applicable
 */
static void MmwDemo_dssMmwaveConfigCallbackFxn(MMWave_CtrlCfg* ptrCtrlCfg)
{
    /* Save the configuration */
    memcpy((void *)(&gMmwDssMCB.cfg.ctrlCfg), (void *)ptrCtrlCfg, sizeof(MMWave_CtrlCfg));

    gMmwDssMCB.stats.configEvt++;

    /* Post event to notify configuration is done */
    Event_post(gMmwDssMCB.eventHandle, MMWDEMO_CONFIG_EVT);

    return;
}

/**
 *  @b Description
 *  @n
 *      Registered Start callback function on DSS which is invoked by MMWAVE library
 *    when the remote side has started mmWaveLink and BSS. This Callback function passes
 *    the event to DataPath task.
 *
 *  @retval
 *      Not applicable
 */
static void MmwDemo_dssMmwaveStartCallbackFxn(void)
{
    gMmwDssMCB.stats.startEvt++;

    /* Post event to start is done */
    Event_post(gMmwDssMCB.eventHandle, MMWDEMO_START_EVT);
}

/**
 *  @b Description
 *  @n
 *      Registered Start callback function on DSS which is invoked by MMWAVE library
 *    when the remote side has stop mmWaveLink and BSS. This Callback function passes
 *    the event to DataPath task.
 *
 *  @retval
 *      Not applicable
 */
static void MmwDemo_dssMmwaveStopCallbackFxn(void)
{
    gMmwDssMCB.stats.stopEvt++;

    /* Post event to stop is done */
    Event_post(gMmwDssMCB.eventHandle, MMWDEMO_STOP_EVT);
}


/**
 *  @b Description
 *  @n
 *      Clock Expiry function. This is used when STOP command is received
 *      from MSS and DSS needs to wait atleast one frame to make sure BSS
 *      events are consumed.
 *
 *  @retval
 *      Not applicable
 */
void MmwDemo_dssFrameClkExpire(UArg arg)
{
    /* Post event to complete stop operation, if pending */
    if (gMmwDssMCB.state == MmwDemo_DSS_STATE_STOP_PENDING)
    {
        Event_post(gMmwDssMCB.eventHandle, MMWDEMO_STOP_COMPLETE_EVT);
    }
}

/**
 *  @b Description
 *  @n
 *      Function to send a message to peer through Mailbox virtural channel
 *
 *  @param[in]  message
 *      Pointer to the Captuere demo message.
 *
 *  @retval
 *      Success    - 0
 *      Fail       < -1
 */
static int32_t MmwDemo_mboxWrite(MmwDemo_message    *message)
{
    int32_t                  retVal = -1;

    retVal = Mailbox_write (gMmwDssMCB.peerMailbox, (uint8_t*)message, sizeof(MmwDemo_message));
    if (retVal == sizeof(MmwDemo_message))
    {
        retVal = 0;
    }
    return retVal;
}

/**
 *  @b Description
 *  @n
 *      The Task is used to handle  the mmw demo messages received from Mailbox virtual channel.
 *
 *  @param[in]  arg0
 *      arg0 of the Task. Not used
 *  @param[in]  arg1
 *      arg1 of the Task. Not used
 *
 *  @retval
 *      Not Applicable.
 */
static void MmwDemo_mboxReadTask(UArg arg0, UArg arg1)
{
    MmwDemo_message      message;
    int32_t              retVal = 0;

    /* wait for new message and process all the messsages received from the peer */
    while(1)
    {
        Semaphore_pend(gMmwDssMCB.mboxSemHandle, BIOS_WAIT_FOREVER);

        /* Read the message from the peer mailbox: We are not trying to protect the read
         * from the peer mailbox because this is only being invoked from a single thread */
        retVal = Mailbox_read(gMmwDssMCB.peerMailbox, (uint8_t*)&message, sizeof(MmwDemo_message));
        if (retVal < 0)
        {
            /* Error: Unable to read the message. Setup the error code and return values */
            System_printf ("Error: Mailbox read failed [Error code %d]\n", retVal);
        }
        else if (retVal == 0)
        {
            /* We are done: There are no messages available from the peer execution domain. */
            continue;
        }
        else
        {
            /* Flush out the contents of the mailbox to indicate that we are done with the message. This will
             * allow us to receive another message in the mailbox while we process the received message. */
            Mailbox_readFlush (gMmwDssMCB.peerMailbox);

            /* Process the received message: */
            switch (message.type)
            {
                case MMWDEMO_MSS2DSS_GUIMON_CFG:
                {
                    /* Save guimon configuration */
                    memcpy((void *)&gMmwDssMCB.cfg.guiMonSel, (void *)&message.body.guiMonSel, sizeof(MmwDemo_GuiMonSel));
                    break;
                }
                case MMWDEMO_MSS2DSS_CFAR_RANGE_CFG:
                {
                    /* Save cfarRange configuration */
                    memcpy((void *)&gMmwDssMCB.dataPathObj.cfarCfgRange,
                           (void *)&message.body.cfarCfg, sizeof(MmwDemo_CfarCfg));
                    break;
                }
                case MMWDEMO_MSS2DSS_CFAR_DOPPLER_CFG:
                {
                    /* Save cfarDoppler configuration */
                    memcpy((void *)&gMmwDssMCB.dataPathObj.cfarCfgDoppler,
                           (void *)&message.body.cfarCfg, sizeof(MmwDemo_CfarCfg));
                    break;
                }
                case MMWDEMO_MSS2DSS_PEAK_GROUPING_CFG:
                {
                    /* Save Peak grouping configuration */
                    memcpy((void *)&gMmwDssMCB.dataPathObj.peakGroupingCfg,
                           (void *)&message.body.peakGroupingCfg, sizeof(MmwDemo_PeakGroupingCfg));
                    break;
                }
                case MMWDEMO_MSS2DSS_MULTI_OBJ_BEAM_FORM:
                {
                    /* Save multi object beam forming configuration */
                    memcpy((void *)&gMmwDssMCB.dataPathObj.multiObjBeamFormingCfg,
                           (void *)&message.body.multiObjBeamFormingCfg, sizeof(MmwDemo_MultiObjBeamFormingCfg));
                    break;
                }
                case MMWDEMO_MSS2DSS_CALIB_DC_RANGE_SIG:
                {
                    /* Save  of DC range antenna signature configuration */
                    memcpy((void *)&gMmwDssMCB.dataPathObj.calibDcRangeSigCfg,
                           (void *)&message.body.calibDcRangeSigCfg, sizeof(MmwDemo_CalibDcRangeSigCfg));
                    gMmwDssMCB.dataPathObj.dcRangeSigCalibCntr = 0;
                    gMmwDssMCB.dataPathObj.log2NumAvgChirps = (uint32_t) log2sp(gMmwDssMCB.dataPathObj.calibDcRangeSigCfg.numAvgChirps);
                    break;
                }
                case MMWDEMO_MSS2DSS_DETOBJ_SHIPPED:
                {
                    gMmwDssMCB.dataPathObj.timingInfo.transmitOutputCycles = Cycleprofiler_getTimeStamp() - gMmwDssMCB.dataPathObj.timingInfo.interFrameProcessingEndTime;
                    gMmwDssMCB.dataPathObj.interFrameProcToken--;

                    gMmwDssMCB.loggingBufferAvailable = 1;

                    /* Post event to complete stop operation, if pending */
                    if (gMmwDssMCB.state == MmwDemo_DSS_STATE_STOP_PENDING)
                    {
                        Event_post(gMmwDssMCB.eventHandle, MMWDEMO_STOP_COMPLETE_EVT);
                    }
                    break;
                }
                case MMWDEMO_MSS2DSS_SET_DATALOGGER:
                {
                    gMmwDssMCB.cfg.dataLogger = message.body.dataLogger;
                    break;
                }
                default:
                {
                    /* Message not support */
                    System_printf ("Error: unsupport Mailbox message id=%d\n", message.type);
                    DebugP_assert(0);
                    break;
                }
            }
        }
    }
}

/**
 *  @b Description
 *  @n
 *      This function is a callback funciton that invoked when a message is received from the peer.
 *
 *  @param[in]  handle
 *      Handle to the Mailbox on which data was received
 *  @param[in]  peer
 *      Peer from which data was received

 *  @retval
 *      Not Applicable.
 */
void MmwDemo_mboxCallback
(
    Mailbox_Handle  handle,
    Mailbox_Type    peer
)
{
    /* Message has been received from the peer endpoint. Wakeup the mmWave thread to process
     * the received message. */
    Semaphore_post (gMmwDssMCB.mboxSemHandle);
}



/**
 *  @b Description
 *  @n
 *      Function to send detected objects to MSS logger.
 *
 *  @param[in]  ptrOutputBuffer
 *      Pointer to the output buffer
 *  @param[in]  outputBufSize
 *      Size of the output buffer
 *  @param[in]  obj
 *      Handle to the Data Path Object
 *
 *  @retval
 *      =0    Success
 *      <0    Failed
 */

int32_t MmwDemo_dssSendProcessOutputToMSS
(
    uint8_t           *ptrHsmBuffer,
    uint32_t           outputBufSize,
    MmwDemo_DSS_DataPathObj   *obj
)
{
    uint32_t            i;
    uint8_t             *ptrCurrBuffer;
    uint32_t            totalHsmSize = 0;
    uint32_t            totalPacketLen = sizeof(MmwDemo_output_message_header);
    uint32_t            itemPayloadLen;
    int32_t             retVal = 0;
    MmwDemo_message     message;
    MmwDemo_GuiMonSel   *pGuiMonSel;
    uint32_t            tlvIdx = 0;

    /* Get Gui Monitor configuration */
    pGuiMonSel = &gMmwDssMCB.cfg.guiMonSel;

    /* Validate input params */
    if(ptrHsmBuffer == NULL)
    {
        retVal = -1;
        goto Exit;
    }


    /* Clear message to MSS */
    memset((void *)&message, 0, sizeof(MmwDemo_message));
    message.type = MMWDEMO_DSS2MSS_DETOBJ_READY;
    /* Header: */
    message.body.detObj.header.platform = 0xA1642;
    message.body.detObj.header.magicWord[0] = 0x0102;
    message.body.detObj.header.magicWord[1] = 0x0304;
    message.body.detObj.header.magicWord[2] = 0x0506;
    message.body.detObj.header.magicWord[3] = 0x0708;
    message.body.detObj.header.numDetectedObj = obj->numDetObj;
    message.body.detObj.header.version =    MMWAVE_SDK_VERSION_BUILD |   //DEBUG_VERSION
                                            (MMWAVE_SDK_VERSION_BUGFIX << 8) |
                                            (MMWAVE_SDK_VERSION_MINOR << 16) |
                                            (MMWAVE_SDK_VERSION_MAJOR << 24);

    /* Set pointer to HSM buffer */
    ptrCurrBuffer = ptrHsmBuffer;

    /* Put detected Objects in HSM buffer: sizeof(MmwDemo_objOut_t) * numDetObj  */
    if ((pGuiMonSel->detectedObjects == 1) && (obj->numDetObj > 0))
    {
        /* Add objects descriptor */
        MmwDemo_output_message_dataObjDescr descr;
        descr.numDetetedObj = obj->numDetObj;
        descr.xyzQFormat = obj->xyzOutputQFormat;
        itemPayloadLen = sizeof(MmwDemo_output_message_dataObjDescr);
        totalHsmSize += itemPayloadLen;
        if(totalHsmSize > outputBufSize)
        {
            retVal = -1;
            goto Exit;
        }
        memcpy(ptrCurrBuffer, (void *)&descr, itemPayloadLen);

        /* Add array of objects */
        itemPayloadLen = sizeof(MmwDemo_detectedObj) * obj->numDetObj;
        totalHsmSize += itemPayloadLen;
        if(totalHsmSize > outputBufSize)
        {
            retVal = -1;
            goto Exit;
        }
        memcpy(&ptrCurrBuffer[sizeof(MmwDemo_output_message_dataObjDescr)], (void *)obj->detObj2D, itemPayloadLen);

        message.body.detObj.tlv[tlvIdx].length = itemPayloadLen + sizeof(MmwDemo_output_message_dataObjDescr);
        message.body.detObj.tlv[tlvIdx].type = MMWDEMO_OUTPUT_MSG_DETECTED_POINTS;
        message.body.detObj.tlv[tlvIdx].address = (uint32_t) ptrCurrBuffer;
        tlvIdx++;

        /* Incrementing pointer to HSM buffer */
        ptrCurrBuffer += itemPayloadLen + sizeof(MmwDemo_output_message_dataObjDescr);
        totalPacketLen += sizeof(MmwDemo_output_message_tl) + itemPayloadLen + sizeof(MmwDemo_output_message_dataObjDescr);
    }

    /* Sending range profile:  2bytes * numRangeBins */
    if (pGuiMonSel->logMagRange == 1)
    {
        itemPayloadLen = sizeof(uint16_t) * obj->numRangeBins;
        totalHsmSize += itemPayloadLen;
        if(totalHsmSize > outputBufSize)
        {
            retVal = -1;
            goto Exit;
        }

        uint16_t *ptrMatrix = (uint16_t *)ptrCurrBuffer;
        for(i = 0; i < obj->numRangeBins; i++)
        {
            ptrMatrix[i] = obj->detMatrix[i*obj->numDopplerBins];
        }

        message.body.detObj.tlv[tlvIdx].length = itemPayloadLen;
        message.body.detObj.tlv[tlvIdx].type = MMWDEMO_OUTPUT_MSG_RANGE_PROFILE;
        message.body.detObj.tlv[tlvIdx].address = (uint32_t) ptrCurrBuffer;
        tlvIdx++;

        /* Incrementing pointer to HSM buffer */
        ptrCurrBuffer = (uint8_t *)((uint32_t)ptrHsmBuffer + totalHsmSize);
        totalPacketLen += sizeof(MmwDemo_output_message_tl) + itemPayloadLen;
   }

    /* Sending range profile:  2bytes * numRangeBins */
    if (pGuiMonSel->noiseProfile == 1)
    {
        uint32_t maxDopIdx = obj->numDopplerBins/2 -1;
        itemPayloadLen = sizeof(uint16_t) * obj->numRangeBins;
        totalHsmSize += itemPayloadLen;
        if(totalHsmSize > outputBufSize)
        {
            retVal = -1;
            goto Exit;
        }

        uint16_t *ptrMatrix = (uint16_t *)ptrCurrBuffer;
        for(i = 0; i < obj->numRangeBins; i++)
        {
            ptrMatrix[i] = obj->detMatrix[i*obj->numDopplerBins + maxDopIdx];
        }

        message.body.detObj.tlv[tlvIdx].length = itemPayloadLen;
        message.body.detObj.tlv[tlvIdx].type = MMWDEMO_OUTPUT_MSG_NOISE_PROFILE;
        message.body.detObj.tlv[tlvIdx].address = (uint32_t) ptrCurrBuffer;
        tlvIdx++;

        /* Incrementing pointer to HSM buffer */
        ptrCurrBuffer = (uint8_t *)((uint32_t)ptrHsmBuffer + totalHsmSize);
        totalPacketLen += sizeof(MmwDemo_output_message_tl) + itemPayloadLen;
   }

    /* Sending range Azimuth Heat Map */
    if (pGuiMonSel->rangeAzimuthHeatMap == 1)
    {
        itemPayloadLen = obj->numRangeBins * obj->numVirtualAntAzim * sizeof(cmplx16ImRe_t);
        message.body.detObj.tlv[tlvIdx].length = itemPayloadLen;
        message.body.detObj.tlv[tlvIdx].type = MMWDEMO_OUTPUT_MSG_AZIMUT_STATIC_HEAT_MAP;
        message.body.detObj.tlv[tlvIdx].address = (uint32_t) obj->azimuthStaticHeatMap;
        tlvIdx++;

        totalPacketLen += sizeof(MmwDemo_output_message_tl) + itemPayloadLen;
    }


    /* Sending range Doppler Heat Map  */
    if (pGuiMonSel->rangeDopplerHeatMap == 1)
    {
        itemPayloadLen = obj->numRangeBins * obj->numDopplerBins * sizeof(uint16_t);
        message.body.detObj.tlv[tlvIdx].length = itemPayloadLen;
        message.body.detObj.tlv[tlvIdx].type = MMWDEMO_OUTPUT_MSG_RANGE_DOPPLER_HEAT_MAP;
        message.body.detObj.tlv[tlvIdx].address = (uint32_t) obj->detMatrix;
        tlvIdx++;

        totalPacketLen += sizeof(MmwDemo_output_message_tl) + itemPayloadLen;
    }

    /* Sending stats information  */
    if (pGuiMonSel->statsInfo == 1)
    {
        MmwDemo_output_message_stats stats;
        itemPayloadLen = sizeof(MmwDemo_output_message_stats);
        totalHsmSize += itemPayloadLen;
        if(totalHsmSize > outputBufSize)
        {
            retVal = -1;
            goto Exit;
        }

        stats.interChirpProcessingMargin = (uint32_t) (obj->timingInfo.chirpProcessingEndMarginMin/DSP_CLOCK_MHZ);
        stats.interFrameProcessingMargin = (uint32_t) (obj->timingInfo.interFrameProcessingEndMargin/DSP_CLOCK_MHZ);
        stats.interFrameProcessingTime = (uint32_t) (obj->timingInfo.interFrameProcCycles/DSP_CLOCK_MHZ);
        stats.transmitOutputTime = (uint32_t) (obj->timingInfo.transmitOutputCycles/DSP_CLOCK_MHZ);
        stats.activeFrameCPULoad = obj->timingInfo.activeFrameCPULoad;
        stats.interFrameCPULoad = obj->timingInfo.interFrameCPULoad;
        memcpy(ptrCurrBuffer, (void *)&stats, itemPayloadLen);

        message.body.detObj.tlv[tlvIdx].length = itemPayloadLen;
        message.body.detObj.tlv[tlvIdx].type = MMWDEMO_OUTPUT_MSG_STATS;
        message.body.detObj.tlv[tlvIdx].address = (uint32_t) ptrCurrBuffer;;
        tlvIdx++;

        /* Incrementing pointer to HSM buffer */
        ptrCurrBuffer = (uint8_t *)((uint32_t)ptrHsmBuffer + totalHsmSize);
        totalPacketLen += sizeof(MmwDemo_output_message_tl) + itemPayloadLen;
    }

    if( retVal == 0)
    {
        message.body.detObj.header.numTLVs = tlvIdx;
        /* Round up packet length to multiple of MMWDEMO_OUTPUT_MSG_SEGMENT_LEN */
        message.body.detObj.header.totalPacketLen = MMWDEMO_OUTPUT_MSG_SEGMENT_LEN *
                ((totalPacketLen + (MMWDEMO_OUTPUT_MSG_SEGMENT_LEN-1))/MMWDEMO_OUTPUT_MSG_SEGMENT_LEN);
        message.body.detObj.header.timeCpuCycles =  Cycleprofiler_getTimeStamp();
        message.body.detObj.header.frameNumber = gMmwDssMCB.stats.frameStartIntCounter;
        if (MmwDemo_mboxWrite(&message) != 0)
        {
            retVal = -1;
        }
    }
Exit:
    return retVal;
}

/**
 *  @b Description
 *  @n
 *      Function to send data path detection output.
 *
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_dssDataPathOutputLogging(MmwDemo_DSS_DataPathObj   * dataPathObj)
{
        /* Sending detected objects to logging buffer and shipped out from MSS UART */
        if (gMmwDssMCB.loggingBufferAvailable == 1)
        {
            /* Set the logging buffer available flag to be 0 */
            gMmwDssMCB.loggingBufferAvailable = 0;

            /* Save output in logging buffer - HSRAM memory and a message is sent to MSS to notify
               logging buffer is ready */
        if (MmwDemo_dssSendProcessOutputToMSS((uint8_t *)&gHSRam,
                                             (uint32_t)SOC_XWR16XX_DSS_HSRAM_SIZE,
                                             dataPathObj) < 0)
            {
                /* Increment logging error */
                gMmwDssMCB.stats.detObjLoggingErr++;
        }
    }
        else
        {
            /* Logging buffer is not available, skip saving detected objectes to logging buffer */
            gMmwDssMCB.stats.detObjLoggingSkip++;
        }
}

/**
 *  @b Description
 *  @n
 *      Function to do Data Path Initialization on DSS.
 *
 *  @retval
 *      Not Applicable.
 */
static int32_t MmwDemo_dataPathAdcBufInit(MmwDemo_DSS_DataPathObj *obj)
{
    ADCBuf_Params       ADCBufparams;

    /*****************************************************************************
     * Initialize ADCBUF driver
     *****************************************************************************/
    ADCBuf_init();

    /* ADCBUF Params initialize */
    ADCBuf_Params_init(&ADCBufparams);
    ADCBufparams.chirpThresholdPing = 1;
    ADCBufparams.chirpThresholdPong = 1;
    ADCBufparams.continousMode  = 0;

    /* Open ADCBUF driver */
    obj->adcbufHandle = ADCBuf_open(0, &ADCBufparams);
    if (obj->adcbufHandle == NULL)
    {
        System_printf("Error: MMWDemoDSS Unable to open the ADCBUF driver\n");
        return -1;
    }
    System_printf("Debug: MMWDemoDSS ADCBUF Instance(0) %p has been opened successfully\n", obj->adcbufHandle);

    return 0;
}

/**
 *  @b Description
 *  @n
 *      Performs linker generated copy table copy using EDMA. Currently this is
 *      used to page in fast code from L3 to L1PSRAM.
 *  @param[in]  handle EDMA handle
 *  @param[in]  tp Pointer to copy table
 *
 *  @retval
 *      Not Applicable.
 */
static void MmwDemo_copyTable(EDMA_Handle handle, COPY_TABLE *tp)
{
    uint16_t i;
    COPY_RECORD crp;
    uint32_t loadAddr;
    uint32_t runAddr;

    for (i = 0; i < tp->num_recs; i++)
    {
        crp = tp->recs[i];
        loadAddr = (uint32_t)crp.load_addr;
        runAddr = (uint32_t)crp.run_addr;

        /* currently we use only one count of EDMA which is 16-bit so we cannot
           handle tables bigger than 64 KB */
        DebugP_assert(crp.size <= 65536U);

        if (crp.size)
        {
            MmwDemo_edmaBlockCopy(handle, loadAddr, runAddr, crp.size);
        }
    }
}

/**
 *  @b Description
 *  @n
 *      Performs simple block copy using EDMA. Used for the purpose of copying
 *      linker table for L3 to L1PSRAM copy. memcpy cannot be used because there is
 *      no data bus access to L1PSRAM.
 *
 *  @param[in]  handle EDMA handle
 *  @param[in]  loadAddr load address
 *  @param[in]  runAddr run address
 *  @param[in]  size size in bytes
 *
 *  @retval
 *      Not Applicable.
 */
static void MmwDemo_edmaBlockCopy(EDMA_Handle handle, uint32_t loadAddr, uint32_t runAddr, uint16_t size)
{
    EDMA_channelConfig_t config;
    volatile bool isTransferDone;

    config.channelId = EDMA_TPCC0_REQ_FREE_0;
    config.channelType = (uint8_t)EDMA3_CHANNEL_TYPE_DMA;
    config.paramId = (uint16_t)EDMA_TPCC0_REQ_FREE_0;
    config.eventQueueId = 0;

    config.paramSetConfig.sourceAddress = (uint32_t) SOC_translateAddress((uint32_t)loadAddr,
        SOC_TranslateAddr_Dir_TO_EDMA, NULL);
    config.paramSetConfig.destinationAddress = (uint32_t) SOC_translateAddress((uint32_t)runAddr,
        SOC_TranslateAddr_Dir_TO_EDMA, NULL);

    config.paramSetConfig.aCount = size;
    config.paramSetConfig.bCount = 1U;
    config.paramSetConfig.cCount = 1U;
    config.paramSetConfig.bCountReload = 0U;

    config.paramSetConfig.sourceBindex = 0U;
    config.paramSetConfig.destinationBindex = 0U;

    config.paramSetConfig.sourceCindex = 0U;
    config.paramSetConfig.destinationCindex = 0U;

    config.paramSetConfig.linkAddress = EDMA_NULL_LINK_ADDRESS;
    config.paramSetConfig.transferType = (uint8_t)EDMA3_SYNC_A;
    config.paramSetConfig.transferCompletionCode = (uint8_t) EDMA_TPCC0_REQ_FREE_0;
    config.paramSetConfig.sourceAddressingMode = (uint8_t) EDMA3_ADDRESSING_MODE_LINEAR;
    config.paramSetConfig.destinationAddressingMode = (uint8_t) EDMA3_ADDRESSING_MODE_LINEAR;

    /* don't care because of linear addressing modes above */
    config.paramSetConfig.fifoWidth = (uint8_t) EDMA3_FIFO_WIDTH_8BIT;

    config.paramSetConfig.isStaticSet = false;
    config.paramSetConfig.isEarlyCompletion = false;
    config.paramSetConfig.isFinalTransferInterruptEnabled = true;
    config.paramSetConfig.isIntermediateTransferInterruptEnabled = false;
    config.paramSetConfig.isFinalChainingEnabled = false;
    config.paramSetConfig.isIntermediateChainingEnabled = false;
    config.transferCompletionCallbackFxn = NULL;
    config.transferCompletionCallbackFxnArg = NULL;

    if (EDMA_configChannel(handle, &config, false) != EDMA_NO_ERROR)
    {
        DebugP_assert(0);
    }

    if (EDMA_startDmaTransfer(handle, config.channelId) != EDMA_NO_ERROR)
    {
        DebugP_assert(0);
    }

    /* wait until transfer done */
    do {
        if (EDMA_isTransferComplete(handle,
                config.paramSetConfig.transferCompletionCode,
                (bool *)&isTransferDone) != EDMA_NO_ERROR)
        {
            DebugP_assert(0);
        }
    } while (isTransferDone == false);

    /* make sure to disable channel so it is usable later */
    EDMA_disableChannel(handle, config.channelId, config.channelType);
}

/**
 *  @b Description
 *  @n
 *      Function to do Data Path Initialization on DSS.
 *
 *  @retval
 *      Not Applicable.
 */
int32_t MmwDemo_dssDataPathInit(void)
{
    MmwDemo_DSS_DataPathObj *obj;
    int32_t retVal;
    SOC_SysIntListenerCfg  socIntCfg;
    int32_t errCode;
    Clock_Params clkParams;

    /* Get DataPath Object handle */
    obj = &gMmwDssMCB.dataPathObj;

    /* Initialize entire data path object to a known state */
    memset((void *)obj, 0, sizeof(MmwDemo_DSS_DataPathObj));

    MmwDemo_dataPathInit1Dstate(obj);
    retVal = MmwDemo_dataPathInitEdma(obj);
    if (retVal < 0)
    {
        return -1;
    }

    /* Copy code from L3 to L1PSRAM, this code related to data path processing */
    MmwDemo_copyTable(obj->edmaHandle[0], &_MmwDemo_fastCode_L1PSRAM_copy_table);

    retVal = MmwDemo_dataPathAdcBufInit(obj);
    if (retVal < 0)
    {
        return -1;
    }

    /* Register chirp interrupt listener */
    socIntCfg.systemInterrupt  = SOC_XWR16XX_DSS_INTC_EVENT_CHIRP_AVAIL;
    socIntCfg.listenerFxn      = MmwDemo_dssChirpIntHandler;
    socIntCfg.arg              = (uintptr_t)NULL;
    if (SOC_registerSysIntListener(gMmwDssMCB.socHandle, &socIntCfg, &errCode) == NULL)
    {
        System_printf("Error: Unable to register chirp interrupt listener , error = %d\n", errCode);
        return -1;
    }

    /* Register frame start interrupt listener */
    socIntCfg.systemInterrupt  = SOC_XWR16XX_DSS_INTC_EVENT_FRAME_START;
    socIntCfg.listenerFxn      = MmwDemo_dssFrameStartIntHandler;
    socIntCfg.arg              = (uintptr_t)NULL;
    if (SOC_registerSysIntListener(gMmwDssMCB.socHandle, &socIntCfg, &errCode) == NULL)
    {
        System_printf("Error: Unable to register frame start interrupt listener , error = %d\n", errCode);
        return -1;
    }

    /* dont start the clock; just create the handle */
    Clock_Params_init( &clkParams );
    gMmwDssMCB.frameClkHandle = Clock_create((Clock_FuncPtr)MmwDemo_dssFrameClkExpire,
                                                0, &clkParams, NULL);
    if (gMmwDssMCB.frameClkHandle == NULL)
    {
        System_printf("Error: Unable to create a clock\n");
        return -1;
    }

    /* Initialize detected objects logging */
    gMmwDssMCB.loggingBufferAvailable = 1;


    return 0;



}

/**
 *  @b Description
 *  @n
 *      Function to configure ADCBUF driver based on CLI inputs.
 *  @param[out] numRxChannels Number of receive channels.
 *
 *  @retval
 *      Not Applicable.
 */
static int32_t MmwDemo_dssDataPathConfigAdcBuf(MmwDemo_DSS_DataPathObj *ptrDataPathObj)
{
    ADCBuf_dataFormat   dataFormat;
    ADCBuf_RxChanConf   rxChanConf;
    int32_t             retVal;
    uint8_t             channel;
    uint8_t             numBytePerSample = 0;
    MMWave_CtrlCfg      *ptrCtrlCfg;
    uint32_t            chirpThreshold;
    uint32_t            rxChanMask = 0xF;

    /* Get data path object and control configuration */
    ptrCtrlCfg   = &gMmwDssMCB.cfg.ctrlCfg;

    /*****************************************************************************
     * Data path :: ADCBUF driver Configuration
     *****************************************************************************/
    /* Validate the adcFmt */
    if(ptrCtrlCfg->u.fullControlCfg.adcbufCfg.adcFmt == 1)
    {
        /* Real dataFormat has 2 bytes */
        numBytePerSample =  2;
    }
    else if(ptrCtrlCfg->u.fullControlCfg.adcbufCfg.adcFmt == 0)
    {
        /* Complex dataFormat has 4 bytes */
        numBytePerSample =  4;
    }
    else
    {
        DebugP_assert(0); /* Data format not supported */
    }

    /* On XWR16xx only channel non-interleaved mode is supported */
    if(ptrCtrlCfg->u.fullControlCfg.adcbufCfg.chInterleave != 1)
    {
        DebugP_assert(0); /* Not supported */
    }

    /* Populate data format from configuration */
    dataFormat.adcOutFormat       = ptrCtrlCfg->u.fullControlCfg.adcbufCfg.adcFmt;
    dataFormat.channelInterleave  = ptrCtrlCfg->u.fullControlCfg.adcbufCfg.chInterleave;
    dataFormat.sampleInterleave   = ptrCtrlCfg->u.fullControlCfg.adcbufCfg.iqSwapSel;


    /* Disable all ADCBuf channels */
    if ((retVal = ADCBuf_control(ptrDataPathObj->adcbufHandle, ADCBufMMWave_CMD_CHANNEL_DISABLE, (void *)&rxChanMask)) < 0)
    {
       System_printf("Error: Disable ADCBuf channels failed with [Error=%d]\n", retVal);
       return retVal;
    }

    retVal = ADCBuf_control(ptrDataPathObj->adcbufHandle, ADCBufMMWave_CMD_CONF_DATA_FORMAT, (void *)&dataFormat);
    if (retVal < 0)
    {
        System_printf ("Error: MMWDemoDSS Unable to configure the data formats\n");
        return -1;
    }

    memset((void*)&rxChanConf, 0, sizeof(ADCBuf_RxChanConf));

    /* Enable Rx Channels */
    for (channel = 0; channel < SYS_COMMON_NUM_RX_CHANNEL; channel++)
    {
        if(ptrCtrlCfg->u.fullControlCfg.chCfg.rxChannelEn & (0x1U << channel))
        {
            /* Populate the receive channel configuration: */
            rxChanConf.channel = channel;
            retVal = ADCBuf_control(ptrDataPathObj->adcbufHandle, ADCBufMMWave_CMD_CHANNEL_ENABLE, (void *)&rxChanConf);
            if (retVal < 0)
            {
                System_printf("Error: MMWDemoDSS ADCBuf Control for Channel %d Failed with error[%d]\n", channel, retVal);
                return -1;
            }
            rxChanConf.offset  += ptrDataPathObj->numAdcSamples * numBytePerSample;
        }
    }

    /* ADCBuf control function requires argument alignment at 4 bytes boundary */
    chirpThreshold = ptrCtrlCfg->u.fullControlCfg.adcbufCfg.chirpThreshold;

    /* Set ping/pong chirp threshold: */
    retVal = ADCBuf_control(ptrDataPathObj->adcbufHandle, ADCBufMMWave_CMD_SET_PING_CHIRP_THRESHHOLD,
                            (void *)&chirpThreshold);
    if(retVal < 0)
    {
        System_printf("Error: ADCbuf Ping Chirp Threshold Failed with Error[%d]\n", retVal);
        return -1;
    }
    retVal = ADCBuf_control(ptrDataPathObj->adcbufHandle, ADCBufMMWave_CMD_SET_PONG_CHIRP_THRESHHOLD,
                            (void *)&chirpThreshold);
    if(retVal < 0)
    {
        System_printf("Error: ADCbuf Pong Chirp Threshold Failed with Error[%d]\n", retVal);
        return -1;
    }

    return 0;
}


/**
 *  @b Description
 *  @n
 *      parses Profile, Chirp and Frame config and extracts parameters
 *      needed for processing chain configuration
 */
bool MmwDemo_parseProfileAndChirpConfig(MmwDemo_DSS_DataPathObj *dataPathObj,MMWave_CtrlCfg      *ptrCtrlCfg)
{
    uint16_t    frameChirpStartIdx;
    uint16_t    frameChirpEndIdx;
    int16_t     frameTotalChirps;
    int32_t     errCode;
    uint32_t    profileLoopIdx, chirpLoopIdx;
    bool        foundValidProfile = false;
    uint16_t    channelTxEn = ptrCtrlCfg->u.fullControlCfg.chCfg.txChannelEn;
    uint8_t     channel;
    uint8_t     numRxChannels = 0;

    /* Find total number of Rx Channels */
    for (channel = 0; channel < SYS_COMMON_NUM_RX_CHANNEL; channel++)
    {
        if(ptrCtrlCfg->u.fullControlCfg.chCfg.rxChannelEn & (0x1U << channel))
        {
            /* Track the number of receive channels: */
            numRxChannels += 1;
        }
    }
    dataPathObj->numRxAntennas = numRxChannels;

    /* read frameCfg chirp start/stop*/
    frameChirpStartIdx = ptrCtrlCfg->u.fullControlCfg.u.chirpModeCfg.frameCfg.chirpStartIdx;
    frameChirpEndIdx = ptrCtrlCfg->u.fullControlCfg.u.chirpModeCfg.frameCfg.chirpEndIdx;
    frameTotalChirps = frameChirpEndIdx - frameChirpStartIdx + 1;

    /* since validChirpTxEnBits is static array of 32 */
    DebugP_assert(frameTotalChirps<=32);

    /* loop for profiles and find if it has valid chirps */
    /* we support only one profile in this processing chain */
    for (profileLoopIdx=0;
        ((profileLoopIdx<MMWAVE_MAX_PROFILE)&&(foundValidProfile==false));
        profileLoopIdx++)
    {
        uint32_t    mmWaveNumChirps = 0;
        bool        validProfileHasMIMO=false;
        uint16_t    validProfileTxEn = 0;
        uint16_t    validChirpTxEnBits[32]={0};
        MMWave_ProfileHandle profileHandle;

        profileHandle = ptrCtrlCfg->u.fullControlCfg.u.chirpModeCfg.profileHandle[profileLoopIdx];
        if (profileHandle == NULL)
            continue; /* skip this profile */

        /* get numChirps for this profile; skip error checking */
        MMWave_getNumChirps(profileHandle,&mmWaveNumChirps,&errCode);
        /* loop for chirps and find if it has valid chirps for the frame
           looping around for all chirps in a profile, in case
           there are duplicate chirps
         */
        for (chirpLoopIdx=1;chirpLoopIdx<=mmWaveNumChirps;chirpLoopIdx++)
        {
            MMWave_ChirpHandle chirpHandle;
            /* get handle and read ChirpCfg */
            if (MMWave_getChirpHandle(profileHandle,chirpLoopIdx,&chirpHandle,&errCode)==0)
            {
                rlChirpCfg_t chirpCfg;
                if (MMWave_getChirpCfg(chirpHandle,&chirpCfg,&errCode)==0)
                {
                    uint16_t chirpTxEn = chirpCfg.txEnable;
                    /* do chirps fall in range and has valid antenna enabled */
                    if ((chirpCfg.chirpStartIdx >= frameChirpStartIdx) &&
                        (chirpCfg.chirpEndIdx <= frameChirpEndIdx) &&
                        ((chirpTxEn & channelTxEn) > 0))
                    {
                        uint16_t idx = 0;
                        for (idx=(chirpCfg.chirpStartIdx-frameChirpStartIdx);idx<=(chirpCfg.chirpEndIdx-frameChirpStartIdx);idx++)
                        {
                            validChirpTxEnBits[idx] = chirpTxEn;
                            foundValidProfile = true;
                        }

                    }
                }
            }
        }
        /* now loop through unique chirps and check if we found all of the ones
           needed for the frame and then determine the azimuth antenna
           configuration
         */
        if (foundValidProfile) {
            for (chirpLoopIdx=0;chirpLoopIdx<frameTotalChirps;chirpLoopIdx++)
            {
                bool validChirpHasMIMO=false;
                uint16_t chirpTxEn = validChirpTxEnBits[chirpLoopIdx];
                if (chirpTxEn == 0) {
                    /* this profile doesnt have all the needed chirps */
                    foundValidProfile = false;
                    break;
                }
                validChirpHasMIMO = ((chirpTxEn==0x1) || (chirpTxEn==0x2));
                /* if this is the first chirp, record the chirp's
                   MIMO config as profile's MIMO config. We dont handle intermix
                   at this point */
                if (chirpLoopIdx==0) {
                    validProfileHasMIMO = validChirpHasMIMO;
                }
                /* check the chirp's MIMO config against Profile's MIMO config */
                if (validChirpHasMIMO != validProfileHasMIMO)
                {
                    /* this profile doesnt have all chirps with same MIMO config */
                    foundValidProfile = false;
                    break;
                }
                /* save the antennas actually enabled in this profile */
                validProfileTxEn |= chirpTxEn;
            }
        }

        /* found valid chirps for the frame; mark this profile valid */
        if (foundValidProfile==true) {
            rlProfileCfg_t  profileCfg;
            uint32_t        numTxAntAzim = 0;
            uint32_t        numTxAntElev = 0;

            dataPathObj->validProfileIdx = profileLoopIdx;
            dataPathObj->numTxAntennas = 0;
            if (!validProfileHasMIMO)
            {
                numTxAntAzim=1;
            }
            else
            {
                if (validProfileTxEn & 0x1)
                {
                    numTxAntAzim++;
                }
                if (validProfileTxEn & 0x2)
                {
                    numTxAntAzim++;
                }
            }
            //System_printf("Azimuth Tx: %d (MIMO:%d) \n",numTxAntAzim,validProfileHasMIMO);
            dataPathObj->numTxAntennas       = numTxAntAzim + numTxAntElev;
            dataPathObj->numVirtualAntAzim   = numTxAntAzim * dataPathObj->numRxAntennas;
            dataPathObj->numVirtualAntElev   = numTxAntElev * dataPathObj->numRxAntennas;
            dataPathObj->numVirtualAntennas  = dataPathObj->numVirtualAntAzim + dataPathObj->numVirtualAntElev;

            /* Get the profile configuration: */
            if (MMWave_getProfileCfg (profileHandle,&profileCfg, &errCode) < 0)
            {
                System_printf ("Error: Unable to get the profile configuration [Error code %d]\n", errCode);
                DebugP_assert (0);
                return -1;
            }

            /* multiplicity of 16 due to windowing library function requirement */
            if ((profileCfg.numAdcSamples % 16) != 0)
            {
                System_printf("Number of ADC samples must be multiple of 16\n");
                DebugP_assert(0);
            }
            dataPathObj->numAdcSamples       = profileCfg.numAdcSamples;
            dataPathObj->numRangeBins        = MmwDemo_pow2roundup(dataPathObj->numAdcSamples);
            dataPathObj->numChirpsPerFrame   = (ptrCtrlCfg->u.fullControlCfg.u.chirpModeCfg.frameCfg.chirpEndIdx -
                                                ptrCtrlCfg->u.fullControlCfg.u.chirpModeCfg.frameCfg.chirpStartIdx + 1) *
                                               ptrCtrlCfg->u.fullControlCfg.u.chirpModeCfg.frameCfg.numLoops;
            dataPathObj->numAngleBins        = MMW_NUM_ANGLE_BINS;
            dataPathObj->numDopplerBins      = dataPathObj->numChirpsPerFrame/dataPathObj->numTxAntennas;

            /* multiplicity of 16 due to windowing library function requirement */
            if ((dataPathObj->numDopplerBins % 16) != 0)
            {
                System_printf("Number of Doppler bins must be multiple of 16\n");
                DebugP_assert(0);
            }

            dataPathObj->rangeResolution     = MMWDEMO_SPEED_OF_LIGHT_IN_METERS_PER_SEC * profileCfg.digOutSampleRate * 1e3 /
                                               (2 * profileCfg.freqSlopeConst * ((3.6*1e3*900)/(1U << 26)) * 1e12 * dataPathObj->numRangeBins);
            dataPathObj->xyzOutputQFormat    = (uint32_t) ceil(log10(16./dataPathObj->rangeResolution)/log10(2));
            dataPathObj->multiObjBeamFormingCfg.enabled = MMWDEMO_AZIMUTH_TWO_PEAK_DETECTION_ENABLE;
            dataPathObj->multiObjBeamFormingCfg.multiPeakThrsScal = MMWDEMO_AZIMUTH_TWO_PEAK_THRESHOLD_SCALE;
        }
    }
    return foundValidProfile;
}


/**
 *  @b Description
 *  @n
 *      Function to do Data Path Configuration on DSS.
 *
 *  @retval
 *      Not Applicable.
 */
static int32_t MmwDemo_dssDataPathConfig(void)
{
    int32_t             retVal;
    MMWave_CtrlCfg      *ptrCtrlCfg;
    MmwDemo_DSS_DataPathObj *dataPathObj;

    /* Get data path object and control configuration */
    ptrCtrlCfg   = &gMmwDssMCB.cfg.ctrlCfg;
    dataPathObj  = &gMmwDssMCB.dataPathObj;

    /*****************************************************************************
     * Data path :: Algorithm Configuration
     *****************************************************************************/

    /* Parse the profile and chirp configs and get the valid number of TX Antennas */
    if (MmwDemo_parseProfileAndChirpConfig(dataPathObj,ptrCtrlCfg) == true)
    {
        retVal = MmwDemo_dssDataPathConfigAdcBuf(dataPathObj);
        if (retVal < 0)
        {
            return -1;
        }

        /* Data path configurations */
        MmwDemo_dataPathConfigBuffers(dataPathObj, SOC_XWR16XX_DSS_ADCBUF_BASE_ADDRESS);
        MmwDemo_dataPathConfigAzimuthHeatMap(dataPathObj);
        MmwDemo_dataPathConfigFFTs(dataPathObj);
        MmwDemo_dataPathConfigEdma(dataPathObj);
    }
    else
    {
        /* no valid profile found - assert! */
        DebugP_assert(0);
    }

    return 0;
}

/**
 *  @b Description
 *  @n
 *      Function to start Data Path on DSS.
 *
 *  @retval
 *      Not Applicable.
 */
static int32_t MmwDemo_dssDataPathStart(bool doRFStart)
{
    int32_t    errCode;
    MMWave_CalibrationCfg   calibrationCfg;

    gMmwDssMCB.dataPathObj.chirpProcToken = 0;
    gMmwDssMCB.dataPathObj.interFrameProcToken = 0;

    if (doRFStart)
    {
        /* Initialize the calibration configuration: */
        memset ((void*)&calibrationCfg, 0, sizeof(MMWave_CalibrationCfg));

        /* Populate the calibration configuration: */
        calibrationCfg.enableCalibration    = true;
        calibrationCfg.enablePeriodicity    = true;
        calibrationCfg.periodicTimeInFrames = 10U;

    /* Start the mmWave module: The configuration has been applied successfully. */
        if (MMWave_start (gMmwDssMCB.ctrlHandle, &calibrationCfg, &errCode) < 0)
    {
        /* Error: Unable to start the mmWave control */
        System_printf ("Error: MMWDemoMSS mmWave Start failed [Error code %d]\n", errCode);
        return -1;
    }
    }

    return 0;
}

/**
 *  @b Description
 *  @n
 *      Function to process Data Path events at runtime.
 *
 *  @param[in]  event
 *      Data Path Event
 *
 *  @retval
 *      Not Applicable.
 */
static int32_t MmwDemo_dssDataPathProcessEvents(UInt event)
{
    MmwDemo_DSS_DataPathObj *dataPathObj;
    volatile uint32_t startTime;

    dataPathObj = &gMmwDssMCB.dataPathObj;

    /* Handle dataPath events */
    switch(event)
    {
        case MMWDEMO_CHIRP_EVT:
            /* Increment event stats */
            gMmwDssMCB.stats.chirpEvt++;

            MmwDemo_processChirp(dataPathObj);
            dataPathObj->chirpProcToken--;

            dataPathObj->timingInfo.chirpProcessingEndTime = Cycleprofiler_getTimeStamp();

            if (dataPathObj->chirpCount == 0)
            {
                MmwDemo_waitEndOfChirps(dataPathObj);
                Load_update();
                dataPathObj->timingInfo.activeFrameCPULoad=Load_getCPULoad();

                dataPathObj->cycleLog.interChirpProcessingTime = gCycleLog.interChirpProcessingTime;
                dataPathObj->cycleLog.interChirpWaitTime = gCycleLog.interChirpWaitTime;
                gCycleLog.interChirpProcessingTime = 0;
                gCycleLog.interChirpWaitTime = 0;

                startTime = Cycleprofiler_getTimeStamp();
                MmwDemo_interFrameProcessing(dataPathObj);
                dataPathObj->timingInfo.interFrameProcCycles = Cycleprofiler_getTimeStamp() - startTime;

                dataPathObj->cycleLog.interFrameProcessingTime = gCycleLog.interFrameProcessingTime;
                dataPathObj->cycleLog.interFrameWaitTime = gCycleLog.interFrameWaitTime;
                gCycleLog.interFrameProcessingTime = 0;
                gCycleLog.interFrameWaitTime = 0;

                /* Sending detected objects to logging buffer */
                MmwDemo_dssDataPathOutputLogging (dataPathObj);
                dataPathObj->timingInfo.interFrameProcessingEndTime = Cycleprofiler_getTimeStamp();
            }
            break;

        case MMWDEMO_FRAMESTART_EVT:
            /* Increment event stats */
            gMmwDssMCB.stats.frameStartEvt++;
            Load_update();
            dataPathObj->timingInfo.interFrameCPULoad=Load_getCPULoad();
            DebugP_assert(dataPathObj->chirpCount == 0);
            break;

        case MMWDEMO_BSS_FRAME_TRIGGER_READY_EVT:
            /* Increment event stats */
            gMmwDssMCB.stats.frameTrigEvt++;
            break;

        default:
            break;
    }
    return 0;
}

/**
 *  @b Description
 *  @n
 *      Function to stop Data Path on DSS. Assume BSS has been stopped by mmWave already.
 *      This also sends the STOP done message back to MSS to signal the procssing
 *      chain has come to a stop.
 *
 *  @retval
 *      Not Applicable.
 */
static int32_t MmwDemo_dssDataPathStop(void)
{
    MmwDemo_message     message;
    uint32_t numFrameEvtsHandled = gMmwDssMCB.stats.frameStartEvt;


    /* move to stop state */
    gMmwDssMCB.state = MmwDemo_DSS_STATE_STOPPED;

    /* Update stats */
    gMmwDssMCB.stats.chirpEvt = 0;
    gMmwDssMCB.stats.frameStartEvt = 0;
    gMmwDssMCB.stats.frameTrigEvt = 0;
    gMmwDssMCB.stats.numFailedTimingReports = 0;
    gMmwDssMCB.stats.numCalibrationReports = 0;

    /* send message back to MSS */
    message.type = MMWDEMO_DSS2MSS_STOPDONE;
    MmwDemo_mboxWrite(&message);

    System_printf ("Debug: MMWDemoDSS Data Path stop succeeded stop%d,frames:%d \n",
                    gMmwDssMCB.stats.stopEvt,numFrameEvtsHandled);

    return 0;
}


/**
 *  @b Description
 *  @n
 *      The task is used to provide an execution context for the mmWave
 *      control task
 *
 *  @retval
 *      Not Applicable.
 */
static void MmwDemo_dssMMWaveCtrlTask(UArg arg0, UArg arg1)
{
    int32_t errCode;

    while (1)
    {
        /* Execute the mmWave control module: */
        if (MMWave_execute (gMmwDssMCB.ctrlHandle, &errCode) < 0)
            System_printf ("Error: MMWDemoDSS mmWave control execution failed [Error code %d]\n", errCode);
    }
}

/**
 *  @b Description
 *  @n
 *      Data Path main task that handles events from remote and do dataPath processing.
 *  This task is created when MSS is responsible for the mmwave Link and DSS is responsible
 *  for data path processing.
 *
 *  @retval
 *      Not Applicable.
 */
static void MmwDemo_dssDataPathTask(UArg arg0, UArg arg1)
{
    int32_t       retVal = 0;
    UInt          event;

    /************************************************************************
     * Data Path :: Config
     ************************************************************************/

    /* Waiting for Config event from Remote - MSS */
    Event_pend(gMmwDssMCB.eventHandle, MMWDEMO_CONFIG_EVT, Event_Id_NONE, BIOS_WAIT_FOREVER);
    if ((retVal = MmwDemo_dssDataPathConfig()) < 0 )
    {
        System_printf ("Debug: MMWDemoDSS Data Path config failed with Error[%d]\n",retVal);
        goto exit;
    }

    /************************************************************************
     * Data Path :: Start, mmwaveLink start will be triggered from DSS!
     ************************************************************************/
    if ((retVal = MmwDemo_dssDataPathStart(true)) < 0 )
    {
        System_printf ("Debug: MMWDemoDSS Data Path start failed with Error[%d]\n",retVal);
        goto exit;
    }
    gMmwDssMCB.state = MmwDemo_DSS_STATE_STARTED;

    /************************************************************************
     * Data Path :: Main loop
     ************************************************************************/
    while (1)
    {
        event = Event_pend(gMmwDssMCB.eventHandle,
                          Event_Id_NONE,
                          MMWDEMO_FRAMESTART_EVT | MMWDEMO_CHIRP_EVT |
                          MMWDEMO_STOP_EVT | MMWDEMO_CONFIG_EVT |
                          MMWDEMO_STOP_COMPLETE_EVT | MMWDEMO_START_EVT,
                          BIOS_WAIT_FOREVER);

        if(event & MMWDEMO_STOP_EVT)
        {
            if(gMmwDssMCB.state == MmwDemo_DSS_STATE_STARTED)
            {
                /* Change state to STOP PENDING state */
                gMmwDssMCB.state = MmwDemo_DSS_STATE_STOP_PENDING;

                /* start the clock; this is to handle the case when there
                   are no active chirps and DSP received the STOP request*/
                   Clock_setTimeout(gMmwDssMCB.frameClkHandle,
                            (1+(UInt)(gMmwDssMCB.cfg.ctrlCfg.u.fullControlCfg.u.chirpModeCfg.frameCfg.framePeriodicity/100000)));
                   Clock_start(gMmwDssMCB.frameClkHandle);
            }
            else
            {
                // Ignore the stop event
            }
        }

        /************************************************************************
         * Data Path process frame start event
         ************************************************************************/
        if(event & MMWDEMO_FRAMESTART_EVT)
        {
            if((gMmwDssMCB.state == MmwDemo_DSS_STATE_STARTED) || (gMmwDssMCB.state == MmwDemo_DSS_STATE_STOP_PENDING))
            {
                if ((retVal = MmwDemo_dssDataPathProcessEvents(MMWDEMO_FRAMESTART_EVT)) < 0 )
                {
                    System_printf ("Error: MMWDemoDSS Data Path process frame start event failed with Error[%d]\n",
                                  retVal);
                }
            }
        }

        /************************************************************************
         * Data Path process chirp event
         ************************************************************************/
        if(event & MMWDEMO_CHIRP_EVT)
        {
            if((gMmwDssMCB.state == MmwDemo_DSS_STATE_STARTED) || (gMmwDssMCB.state == MmwDemo_DSS_STATE_STOP_PENDING))
            {
                if ((retVal = MmwDemo_dssDataPathProcessEvents(MMWDEMO_CHIRP_EVT)) < 0 )
                {
                    System_printf ("Error: MMWDemoDSS Data Path process chirp event failed with Error[%d]\n",
                                  retVal);
                }
            }
        }

        /************************************************************************
         * Data Path re-config, only supported reconfiguration in stop state
         ************************************************************************/
        if(event & MMWDEMO_CONFIG_EVT)
        {
            if(gMmwDssMCB.state == MmwDemo_DSS_STATE_STOPPED)
            {
                if ((retVal = MmwDemo_dssDataPathConfig()) < 0 )
                {
                    System_printf ("Debug: MMWDemoDSS Data Path config failed with Error[%d]\n",retVal);
                    goto exit;
                }

                /************************************************************************
                 * Data Path :: Start, mmwaveLink start will be triggered from DSS!
                 ************************************************************************/
                if ((retVal = MmwDemo_dssDataPathStart(true)) < 0 )
                {
                    System_printf ("Error: MMWDemoDSS Data Path start failed with Error[%d]\n",retVal);
                    goto exit;
                }
                gMmwDssMCB.state = MmwDemo_DSS_STATE_STARTED;
            }
            else
            {
                System_printf ("Error: MMWDemoDSS Data Path config event in wrong state[%d]\n", gMmwDssMCB.state);
                goto exit;
            }
        }

        /************************************************************************
         * Quick start after stop
         ************************************************************************/
        if(event & MMWDEMO_START_EVT)
        {
            if(gMmwDssMCB.state == MmwDemo_DSS_STATE_STOPPED)
            {
                /* RF start is done by MSS in this case; so just do DSS start */
                if ((retVal = MmwDemo_dssDataPathStart(false)) < 0 )
                {
                    System_printf ("Error: MMWDemoDSS Data Path start failed with Error[%d]\n",retVal);
                    goto exit;
    }
                gMmwDssMCB.state = MmwDemo_DSS_STATE_STARTED;
            }
            else
            {
                System_printf ("Error: MMWDemoDSS Data Path config event in wrong state[%d]\n", gMmwDssMCB.state);
                goto exit;
            }
        }

        /************************************************************************
         * Data Path process frame start event
         ************************************************************************/
        if(event & MMWDEMO_STOP_COMPLETE_EVT)
        {
            if (gMmwDssMCB.state == MmwDemo_DSS_STATE_STOP_PENDING)
            {
                /************************************************************************
                 * Local Data Path Stop
                 ************************************************************************/
                if ((retVal = MmwDemo_dssDataPathStop()) < 0 )
                {
                    System_printf ("Debug: MMWDemoDSS Data Path stop failed with Error[%d]\n",retVal);
                }
            }
        }
    }
exit:
    System_printf("Debug: MMWDemoDSS Data path exit\n");

}

/**
 *  @b Description
 *  @n
 *      System Initialization Task which initializes the various
 *      components in the system.
 *
 *  @retval
 *      Not Applicable.
 */
static void MmwDemo_dssInitTask(UArg arg0, UArg arg1)
{
    int32_t             errCode;
    MMWave_InitCfg      initCfg;
    Task_Params         taskParams;
    UART_Params         uartParams;
    Semaphore_Params    semParams;
    Mailbox_Config      mboxCfg;
    Error_Block         eb;

    /* Initialize the UART */
    UART_init();

    /* Initialize the Mailbox */
    Mailbox_init(MAILBOX_TYPE_DSS);

    /*****************************************************************************
     * Open & configure the drivers:
     *****************************************************************************/

    /* Setup the default logging UART Parameters */
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.clockFrequency  = DSS_SYS_VCLK;
    uartParams.baudRate        = gMmwDssMCB.cfg.loggingBaudRate;
    uartParams.isPinMuxDone    = 1;

    /* Open the Logging UART Instance: */
    gMmwDssMCB.loggingUartHandle = UART_open(0, &uartParams);
    if (gMmwDssMCB.loggingUartHandle == NULL)
    {
        System_printf("Error: Unable to open the Logging UART Instance\n");
        return;
    }
    System_printf("Debug: Logging UART Instance %p has been opened successfully\n", gMmwDssMCB.loggingUartHandle);

    /*****************************************************************************
     * Create mailbox Semaphore:
     *****************************************************************************/
    /* Create a binary semaphore which is used to handle mailbox interrupt. */
    Semaphore_Params_init(&semParams);
    semParams.mode             = Semaphore_Mode_BINARY;
    gMmwDssMCB.mboxSemHandle = Semaphore_create(0, &semParams, NULL);

    /* Setup the default mailbox configuration */
    Mailbox_Config_init(&mboxCfg);

    /* Setup the configuration: */
    mboxCfg.chType       = MAILBOX_CHTYPE_MULTI;
    mboxCfg.chId         = MAILBOX_CH_ID_0;
    mboxCfg.writeMode    = MAILBOX_MODE_BLOCKING;
    mboxCfg.readMode     = MAILBOX_MODE_CALLBACK;
    mboxCfg.readCallback = &MmwDemo_mboxCallback;

    gMmwDssMCB.peerMailbox = Mailbox_open(MAILBOX_TYPE_MSS, &mboxCfg, &errCode);
    if (gMmwDssMCB.peerMailbox == NULL)
    {
        /* Error: Unable to open the mailbox */
        System_printf("Error: Unable to open the Mailbox to the DSS [Error code %d]\n", errCode);
        return;
    }
    else
    {
        /* Debug Message: */
        System_printf("Debug: DSS Mailbox Handle %p\n", gMmwDssMCB.peerMailbox);

        /* Create task to handle mailbox messges */
        Task_Params_init(&taskParams);
        Task_create(MmwDemo_mboxReadTask, &taskParams, NULL);
    }

    /*****************************************************************************
     * Create Event to handle mmwave callback and system datapath events
     *****************************************************************************/
    /* Default instance configuration params */
    Error_init(&eb);
    gMmwDssMCB.eventHandle = Event_create(NULL, &eb);
    if (gMmwDssMCB.eventHandle == NULL)
    {
        /* FATAL_TBA */
        System_printf("Error: MMWDemoDSS Unable to create an event handle\n");
        return ;
    }
    System_printf("Debug: MMWDemoDSS create event handle succeeded\n");

    /************************************************************************
     * mmwave library initialization
     ************************************************************************/

    /* Populate the init configuration for mmwave library: */
    initCfg.domain                    = MMWave_Domain_DSS;
    initCfg.socHandle                 = gMmwDssMCB.socHandle;
    initCfg.eventFxn                  = MmwDemo_dssMmwaveEventCallbackFxn;
    initCfg.linkCRCCfg.useCRCDriver   = 1U;
    initCfg.linkCRCCfg.crcChannel     = CRC_Channel_CH1;
    initCfg.cfgMode                   = MMWave_ConfigurationMode_FULL;
    initCfg.executionMode             = MMWave_ExecutionMode_COOPERATIVE;
    initCfg.cooperativeModeCfg.cfgFxn = MmwDemo_dssMmwaveConfigCallbackFxn;
    initCfg.cooperativeModeCfg.startFxn = MmwDemo_dssMmwaveStartCallbackFxn;
    initCfg.cooperativeModeCfg.stopFxn = MmwDemo_dssMmwaveStopCallbackFxn;

    /* Initialize and setup the mmWave Control module */
    gMmwDssMCB.ctrlHandle = MMWave_init (&initCfg, &errCode);
    if (gMmwDssMCB.ctrlHandle == NULL)
    {
        /* Error: Unable to initialize the mmWave control module */
        System_printf ("Error: MMWDemoDSS mmWave Control Initialization failed [Error code %d]\n", errCode);
        return;
    }
    System_printf ("Debug: MMWDemoDSS mmWave Control Initialization succeeded\n");

    /******************************************************************************
     * TEST: Synchronization
     * - The synchronization API always needs to be invoked.
     ******************************************************************************/
    while (1)
    {
        int32_t syncStatus;

        /* Get the synchronization status: */
        syncStatus = MMWave_sync (gMmwDssMCB.ctrlHandle , &errCode);
        if (syncStatus < 0)
        {
            /* Error: Unable to synchronize the mmWave control module */
            System_printf ("Error: MMWDemoDSS mmWave Control Synchronization failed [Error code %d]\n", errCode);
            return;
        }
        if (syncStatus == 1)
        {
            /* Synchronization acheived: */
            break;
        }
        /* Sleep and poll again: */
        Task_sleep(1);
    }

    /*****************************************************************************
     * Launch the mmWave control execution task
     * - This should have a higher priroity than any other task which uses the
     *   mmWave control API
     *****************************************************************************/
    Task_Params_init(&taskParams);
    taskParams.priority = 6;
    taskParams.stackSize = 4*1024;
    Task_create(MmwDemo_dssMMWaveCtrlTask, &taskParams, NULL);

    /*****************************************************************************
     * Data path Startup
     *****************************************************************************/
    if ((errCode = MmwDemo_dssDataPathInit()) < 0 )
    {
        System_printf ("Error: MMWDemoDSS Data Path init failed with Error[%d]\n",errCode);
        return;
    }
    System_printf ("Debug: MMWDemoDSS Data Path init succeeded\n");
    gMmwDssMCB.state = MmwDemo_DSS_STATE_INIT;

    /* Start data path task */
    Task_Params_init(&taskParams);
    taskParams.priority = 5;
    taskParams.stackSize = 4*1024;
    Task_create(MmwDemo_dssDataPathTask, &taskParams, NULL);

    System_printf("Debug: MMWDemoDSS initTask exit\n");
    return;
}

/**
 *  @b Description
 *  @n
 *      Entry point into the test code.
 *
 *  @retval
 *      Not Applicable.
 */
int main (void)
{
    Task_Params    taskParams;
    SOC_Cfg        socCfg;
    int32_t        errCode;

    /* Initialize and populate the demo MCB */
    memset ((void*)&gMmwDssMCB, 0, sizeof(MmwDemo_DSS_MCB));

    /* Initialize the SOC confiugration: */
    memset ((void *)&socCfg, 0, sizeof(SOC_Cfg));

    /* Populate the SOC configuration: */
    socCfg.clockCfg = SOC_SysClock_BYPASS_INIT;

    /* Initialize the SOC Module: This is done as soon as the application is started
     * to ensure that the MPU is correctly configured. */
    gMmwDssMCB.socHandle = SOC_init (&socCfg, &errCode);
    if (gMmwDssMCB.socHandle == NULL)
    {
        System_printf ("Error: SOC Module Initialization failed [Error code %d]\n", errCode);
        return -1;
    }

    /* Initialize the DEMO configuration: */
    gMmwDssMCB.cfg.sysClockFrequency = DSS_SYS_VCLK;
    gMmwDssMCB.cfg.loggingBaudRate   = 921600;

    Cycleprofiler_init();

    /* Initialize the Task Parameters. */
    Task_Params_init(&taskParams);
    taskParams.stackSize = 3*1024;
    Task_create(MmwDemo_dssInitTask, &taskParams, NULL);

    /* Start BIOS */
    BIOS_start();
    return 0;
}

