/**
 *   @file  dss_data_path.h
 *
 *   @brief
 *      This is the data path processing header.
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
#ifndef DSS_DATA_PATH_H
#define DSS_DATA_PATH_H

#include <ti/sysbios/knl/Semaphore.h>

#include <ti/common/sys_common.h>
#include <ti/common/mmwave_error.h>
#include <ti/demo/io_interface/mmw_config.h>
#include <ti/drivers/adcbuf/ADCBuf.h>
#include <ti/drivers/edma/edma.h>
#include <ti/demo/io_interface/detected_obj.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BYTES_PER_SAMP_1D (2*sizeof(int16_t))  /*16 bit real, 16 bit imaginary => 4 bytes */
#define BYTES_PER_SAMP_2D (2*sizeof(int32_t))  /*32 bit real, 32 bit imaginary => 8 bytes */
#define BYTES_PER_SAMP_DET sizeof(uint16_t) /*pre-detection matrix is 16 bit unsigned =>2 bytes*/

//DETECTION (CFAR-CA) related parameters
#define MMW_MAX_OBJ_OUT 100
#define MAX_DET_OBJECTS_RAW 2048 /* same as xwr14xx */
#define DET_THRESH_MULT 25
#define DET_THRESH_SHIFT 5 //DET_THRESH_MULT and DET_THRESH_SHIFT together define the CFAR-CA threshold
#define DET_GUARD_LEN 4 // this is the one sided guard lenght
#define DET_NOISE_LEN 16 //this is the one sided noise length

#define PI_ 3.1415926535897
#define ONE_Q15 (1 << 15)
#define ONE_Q19 (1 << 19)
#define ONE_Q8 (1 << 8)

#define EDMA_INSTANCE_A 0
#define EDMA_INSTANCE_B 1

/*!< Peak grouping scheme of CFAR detected objects based on peaks of neighboring cells taken from detection matrix */
#define MMW_PEAK_GROUPING_DET_MATRIX_BASED 1
/*!< Peak grouping scheme of CFAR detected objects based only on peaks of neighboring cells that are already detected by CFAR */
#define MMW_PEAK_GROUPING_CFAR_PEAK_BASED  2

/*!< cumulative average of left+right */
#define MMW_NOISE_AVG_MODE_CFAR_CA       ((uint8_t)0U)
/*!< cumulative average of the side (left or right) that is greater */
#define MMW_NOISE_AVG_MODE_CFAR_CAGO     ((uint8_t)1U)
/*!< cumulative average of the side (left or right) that is smaller */
#define MMW_NOISE_AVG_MODE_CFAR_CASO     ((uint8_t)2U)

/*! @brief DSP cycle profiling structure to accumulate different
    processing times in chirp and frame processing periods */
typedef struct cycleLog_t_ {
    uint32_t interChirpProcessingTime; /*!< @brief total processing time during all chirps in a frame excluding EDMA waiting time*/
    uint32_t interChirpWaitTime; /*!< @brief total wait time for EDMA data transfer during all chirps in a frame*/
    uint32_t interFrameProcessingTime; /*!< @brief total processing time for 2D and 3D excluding EDMA waiting time*/
    uint32_t interFrameWaitTime; /*!< @brief total wait time for 2D and 3D EDMA data transfer */
} cycleLog_t;




/*!
 *  @brief    Parameters of CFAR detected object during the first round of
 *  CFAR detections
 *
 */
typedef struct MmwDemo_objRaw
{
    uint16_t   rangeIdx;     /*!< @brief Range index */
    uint16_t   dopplerIdx;   /*!< @brief Dopler index */
    uint16_t   peakVal;      /*!< @brief Peak value */
} MmwDemo_objRaw_t;

/*!
 *  @brief    Active Doppler lines, lines (bins) on which the
 *            CFAR detector detected objects during the detections in
 *            Doppler direction
 *
 */
typedef struct MmwDemo_1D_DopplerLines
{
    uint32_t   *dopplerLineMask;      /*!< @brief Doppler line bit mask of active
                                   (CFAR detected) Doppler bins in the first
                                   round of CFAR detections in DOppler direction.
                                   The LSB bit of the first word corresponds to
                                   Doppler bin zero of the range/Doppler
                                   detection matrix*/
    uint16_t   currentIndex;     /*!< @brief starting index for the search
                                   for next active Doppler line */
    uint16_t    dopplerLineMaskLen;   /*!< @brief size of dopplerLineMask array, (number of
                                    32_bit words, for example for Doppler FFT
                                    size of 64 this length is equal to 2)*/
} MmwDemo_1D_DopplerLines_t;

/*!
 *  @brief Timing information
 */
typedef struct MmwDemo_timingInfo
{
    /*! @brief number of processor cycles between frames excluding
           processing time to transmit output on UART */
    uint32_t interFrameProcCycles;

     /*! @brief time to transmit out detection information (in DSP cycles)
           */
    uint32_t transmitOutputCycles;

    /*! @brief Chirp processing end time */
    uint32_t chirpProcessingEndTime;

    /*! @brief Chirp processing end margin in number of cycles before due time
     * to start processing next chirp, minimum value*/
    uint32_t chirpProcessingEndMarginMin;

    /*! @brief Chirp processing end margin in number of cycles before due time
     * to start processing next chirp, maximum value*/
    uint32_t chirpProcessingEndMarginMax;

    /*! @brief Inter frame processing end time */
    uint32_t interFrameProcessingEndTime;

    /*! @brief Inter frame processing end margin in number of cycles before
     * due time to start processing first chirp of the next frame */
    uint32_t interFrameProcessingEndMargin;

    /*! @brief CPU Load during active frame period - i.e. chirping */
    uint32_t activeFrameCPULoad; 

    /*! @brief CPU Load during inter frame period - i.e. after chirps 
     *  are done and before next frame starts */
    uint32_t interFrameCPULoad; 

} MmwDemo_timingInfo_t;


/**
 * @brief
 *  Millimeter Wave Demo Data Path Information.
 *
 * @details
 *  The structure is used to hold all the relevant information for
 *  the data path.
 */
typedef struct MmwDemo_DSS_DataPathObj_t
{
    /*! @brief   Number of receive channels */
    uint32_t numRxAntennas;

    /*! @brief   Chirp Threshold configuration used for ADCBUF driver */
    uint8_t chirpThreshold;

    /*! @brief   Chirp Available Semaphore: */
    Semaphore_Handle  chirpSemHandle;

    /*! @brief   Counter which tracks the number of chirps detected */
    uint32_t chirpInterruptCounter;

    /*! @brief   ADCBUF handle. */
    ADCBuf_Handle adcbufHandle;

    /*! @brief   Handle of the EDMA driver. */
    EDMA_Handle edmaHandle[2];

    /*! @brief   EDMA error Information when there are errors like missing events */
    EDMA_errorInfo_t  EDMA_errorInfo;

    /*! @brief EDMA transfer controller error information. */
    EDMA_transferControllerErrorInfo_t EDMA_transferControllerErrorInfo;

    /*! @brief Semaphore handle for 1D EDMA completion. */
    Semaphore_Handle EDMA_1D_InputDone_semHandle[2];
    /*! @brief Semaphore handle for 1D EDMA completion. */
    Semaphore_Handle EDMA_1D_OutputDone_semHandle[2];
    /*! @brief Semaphore handle for 2D EDMA completion. */
    Semaphore_Handle EDMA_2D_InputDone_semHandle[2];
    /*! @brief Semaphore handle for Detection Matrix completion. */
    Semaphore_Handle EDMA_DetMatrix_semHandle;
    /*! @brief Semaphore handle for Detection Matrix2 completion. */
    Semaphore_Handle EDMA_DetMatrix2_semHandle;
    /*! @brief Semaphore handle for 3D EDMA completion. */
    Semaphore_Handle EDMA_3D_InputDone_semHandle[2];

    /*! @brief pointer to ADC buffer */
    cmplx16ReIm_t *ADCdataBuf;

    /*! @brief twiddle table for 1D FFT */
    cmplx16ReIm_t *twiddle16x16_1D;

    /*! @brief window coefficients for 1D FFT */
    int16_t *window1D;

    /*! @brief ADCBUF input samples in L2 scratch memory */
    cmplx16ReIm_t *adcDataIn;

    /*! @brief 1D FFT output */
    cmplx16ReIm_t *fftOut1D;

    /*! @brief twiddle table for 2D FFT */
    cmplx32ReIm_t *twiddle32x32_2D;

    /*! @brief window coefficients for 2D FFT */
    int32_t *window2D;

    /*! @brief ping pong buffer for 2D from radar Cube */
    cmplx16ReIm_t *dstPingPong;

    /*! @brief window output for 2D FFT */
    cmplx32ReIm_t *windowingBuf2D;

    /*! @brief 2D FFT output */
    cmplx32ReIm_t *fftOut2D;

    /*! @brief log2 absolute computation output buffer */
    uint16_t *log2Abs;

    /*! @brief accumulated sum of log2 absolute over the antennae */
    uint16_t *sumAbs;

    /*! @brief input buffer for CFAR processing from the detection matrix */
    uint16_t *sumAbsRange;

    /*! @brief CFAR output objects index buffer */
    uint16_t *cfarDetObjIndexBuf;

    /*! @brief input for Azimuth FFT */
    cmplx32ReIm_t *azimuthIn;

    /*! @brief output of Azimuth FFT */
    cmplx32ReIm_t *azimuthOut;

    /*! @brief output of Azimuth FFT magnitude squared */
    float   *azimuthMagSqr;

    /*! @brief twiddle factors table for Azimuth FFT */
    cmplx32ReIm_t *azimuthTwiddle32x32;

    /*! @brief Pointer to single point DFT coefficients used for Azimuth processing */
    cmplx16ImRe_t *azimuthModCoefs;

    /*! @brief Pointer to DC range signature compensation buffer */
    cmplx32ImRe_t *dcRangeSigMean;

    /*! @brief DC range signature calibration counter */
    uint32_t dcRangeSigCalibCntr;

    /*! @brief log2 of number of averaged chirps */
    uint32_t log2NumAvgChirps;

    /*! @brief Half bin needed for doppler correction as part of Azimuth processing */
    cmplx16ImRe_t azimuthModCoefsHalfBin;

    /*! @brief Pointer to Radar Cube memory in L3 RAM */
    cmplx16ReIm_t *radarCube;

    /*! @brief Pointer to range/Doppler log2 magnitude detection matrix in L3 RAM */
    uint16_t *detMatrix;

    /*! @brief Pointer to 2D FFT array in range direction, at doppler index 0,
     * for static azimuth heat map */
    cmplx16ImRe_t *azimuthStaticHeatMap;

    /*! @brief noise energy */
    uint32_t noiseEnergy;

    /*! @brief valid Profile index */
    uint32_t validProfileIdx;

    /*! @brief number of transmit antennas */
    uint32_t numTxAntennas;

    /*! @brief number of virtual antennas */
    uint32_t numVirtualAntennas;

    /*! @brief number of virtual azimuth antennas */
    uint32_t numVirtualAntAzim;

    /*! @brief number of virtual elevation antennas */
    uint32_t numVirtualAntElev;

    /*! @brief number of ADC samples */
    uint32_t numAdcSamples;

    /*! @brief number of range bins */
    uint32_t numRangeBins;

    /*! @brief number of chirps per frame */
    uint32_t numChirpsPerFrame;

    /*! @brief number of angle bins */
    uint32_t numAngleBins;

    /*! @brief number of doppler bins */
    uint32_t numDopplerBins;

    /*! @brief number of doppler bins */
    uint32_t log2NumDopplerBins;

    /*! @brief range resolution in meters */
    float rangeResolution;

    /*! @brief Q format of the output x/y/z coordinates */
    uint32_t xyzOutputQFormat;

    /*! @brief Number of detected objects */
    uint32_t numDetObj;

    /*! @brief Number of detected objects */
    uint32_t numDetObjRaw;

    /*! @brief Detected Doppler lines */
	MmwDemo_1D_DopplerLines_t detDopplerLines;

    /*! @brief Detected objects after second pass in Range direction */
    MmwDemo_detectedObj *detObj2D;

    /*! @brief Detected objects before peak grouping */
    MmwDemo_objRaw_t *detObj2DRaw;

    /*! @brief Detected objects azimuth index for debugging */
    uint8_t *detObj2dAzimIdx;

    /*! @brief CFAR configuration in Doppler direction */
    MmwDemo_CfarCfg cfarCfgDoppler;

    /*! @brief CFAR configuration in Range direction */
    MmwDemo_CfarCfg cfarCfgRange;

    /*! @brief Peak grouping configuration */
    MmwDemo_PeakGroupingCfg peakGroupingCfg;

    /*! @brief Multi object beam forming configuration */
    MmwDemo_MultiObjBeamFormingCfg multiObjBeamFormingCfg;

    /*! @brief DC Range antenna signature callibration configuration */
    MmwDemo_CalibDcRangeSigCfg calibDcRangeSigCfg;

    /*! @brief Timing information */
    MmwDemo_timingInfo_t timingInfo;

    /*! @brief chirp counter modulo number of chirps per frame */
    uint32_t chirpCount;

    /*! @brief chirp counter modulo number of tx antennas */
    uint32_t txAntennaCount;

    /*! @brief chirp counter modulo number of Doppler bins */
    uint32_t dopplerBinCount;

    /*! @brief  DSP cycles for chirp and interframe processing and pending
     *          on EDMA data transferes*/
    cycleLog_t cycleLog;

    /*! @brief  Used for checking that chirp processing finshed on time */
    int32_t chirpProcToken;

    /*! @brief  Used for checking that inter frame processing finshed on time */
    int32_t interFrameProcToken;
} MmwDemo_DSS_DataPathObj;

/**
 *  @b Description
 *  @n
 *   Initializes data path state variables for 1D processing.
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_dataPathInit1Dstate(MmwDemo_DSS_DataPathObj *obj);

/**
 *  @b Description
 *  @n
 *   Delete Semaphores which are created in MmwDemo_dataPathInitEdma().
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_dataPathDeleteSemaphore(MmwDemo_DSS_DataPathObj *obj);

/**
 *  @b Description
 *  @n
 *   Initializes EDMA driver.
 *  @retval
 *      Not Applicable.
 */
int32_t MmwDemo_dataPathInitEdma(MmwDemo_DSS_DataPathObj *obj);

/**
 *  @b Description
 *  @n
 *   Configures EDMA driver for all of the data path processing.
 *  @retval
 *      Not Applicable.
 */
int32_t MmwDemo_dataPathConfigEdma(MmwDemo_DSS_DataPathObj *obj);

/**
 *  @b Description
 *  @n
 *   Creates heap in L2 and L3 and allocates data path buffers,
 *   The heap is destroyed at the end of the function.
 *
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_dataPathConfigBuffers(MmwDemo_DSS_DataPathObj *obj, uint32_t adcBufAddress);

/**
 *  @b Description
 *  @n
 *   Configures azimuth heat map related processing.
 *
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_dataPathConfigAzimuthHeatMap(MmwDemo_DSS_DataPathObj *obj);

/**
 *  @b Description
 *  @n
 *   Configures FFTs (twiddle tables etc) involved in 1D, 2D and Azimuth processing.
 *
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_dataPathConfigFFTs(MmwDemo_DSS_DataPathObj *obj);

/**
 *  @b Description
 *  @n
 *  Wait for transfer of data corresponding to the last 2 chirps (ping/pong)
 *  to the radarCube matrix before starting interframe processing.
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_waitEndOfChirps(MmwDemo_DSS_DataPathObj *obj);

/**
 *  @b Description
 *  @n
 *    Chirp processing. It is called from MmwDemo_dssDataPathProcessEvents. It
 *    is executed per chirp
 *
 *  @retval
 *      Not Applicable.
 */
void MmwDemo_processChirp(MmwDemo_DSS_DataPathObj *obj);

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
void MmwDemo_interFrameProcessing(MmwDemo_DSS_DataPathObj *obj);

/**
 *  @b Description
 *  @n
 *      Power of 2 round up function.
 */
uint32_t MmwDemo_pow2roundup (uint32_t x);

/*!*****************************************************************************************************************
 * \brief
 * Function Name       :    mmwavelib_dftSingleBin_1Ant
 *
 * \par
 * <b>Description</b>  :    Calculates single bin DFT. Profile: N/4*5 cycles
 *
 * \param[in]          :    inputBuf    Input with int16 complex samples, real in even, imaginary in odd location
 *
 * \param[in]          :    sincos      Table with sine cosine values, exp(-1j*2*pi*k/N),
 *                                      valuse are int16 in Q15 format, imaginary in even, real in odd location
 *
 * \param[out]         :    outputBuf   Single point DFT value, int32 complex value, real in even, imaginary in odd location
 *
 * \param[in]          :    length      Length of input buffer (size of DFT) must be power of 2
 *
 * \param[in]          :    doppInd     Index value at wich the DFT is calculated
 *
 * \return                  void
 *
 *******************************************************************************************************************
 */

#ifdef __cplusplus
}
#endif

#endif /* DSS_DATA_PATH_H */

