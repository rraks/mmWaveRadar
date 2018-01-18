/**
 *   @file  mss_main.c
 *
 *   @brief
 *     MSS main implementation of the millimeter wave Demo
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
/** @mainpage Millimeter Wave (mmw) Demo
 *
 *  @section intro_sec Introduction
 *
 *  @image html toplevel.png
 *
 *  The millimeter wave demo shows some of the capabilities of the XWR16xx SoC
 *  using the drivers in the mmWave SDK (Software Development Kit).
 *  It allows user to specify the chirping profile and displays the detected
 *  objects and other information in real-time.

 *
 *  Following is a high level description of the features of this demo:
 *  - Be able to specify desired chirping profile through command line interface (CLI) 
 *    on a UART port or through the TI Gallery App - **mmWave Demo Visualizer** - 
 *    that allows user to provide a variety of profile configurations via the UART input port 
 *    and displays the streamed detected output from another UART port in real-time, 
 *    as seen in picture above.
 *  - Some sample profile configurations have been provided in the demo directory that can be
 *    used with CLI directly or via **mmWave Demo Visualizer**:
 *    @verbatim
 mmw/profiles/profile_2d.cfg
 mmw/profiles/profile_2d_srr.cfg
 mmw/profiles/profile_heat_map.cfg
 @endverbatim
 *  - Do 1D, 2D, CFAR and Azimuth processing and stream out velocity
 *    and two spatial coordinates (x,y) of the detected objects in real-time.
 *    The demo can also be configured to do 2D only detection (velocity and x,y coordinates).
 *  - Various display options besides object detection like azimuth heat map and
 *    Doppler-range heat map.
 *  - Illustrates how to configure the various hardware entities (
 *    EDMA, UART) in the AR SoC using the driver software.
 *
 *  @section limit Limitations
 *  
 *  - Because of UART speed limit (< 1 Mbps), the frame time is more restrictive.
 *    For example, for the azimuth and Doppler heat maps for 256 FFT range and
 *    16 point FFT Doppler, it takes about 200 ms to transmit.
 *  - Present implementation in this demo can resolve up to two objects in the azimuth
 *    dimension which have the same range and same velocity. 
 *  - Code will give an error if the requested memory in L3 RAM exceeds its size
 *   (@ref SOC_XWR16XX_DSS_L3RAM_SIZE) due to particular combination  of CLI configuration parameters.
 *  - There is a bias of +8 cm (e.g true is 1 m but measured is 1.08 m) in the range direction
 *    measured with the profile configurations in mmw/gui.
 *    This is likely due to delay in the front-end of the RF chain, which hasn't been
 *    characterized yet.
 *  
 *  @section tasks System Details
 *  The millimeter wave demo runs on both R4F (MSS) and C674x (DSS).
 *  System startup is described in the following diagram:
 *
 *  @image html system_startup.png "System Startup"
 *
 *
 *  @subsection mss_tasks Software Tasks on MSS
 *    The following (SYSBIOS) tasks are running on MSS:
 *     - @ref MmwDemo_mssInitTask. This task is created and launched by @ref main and is a
 *       one-time initialization  task that performs the following sequence:
 *       -# Initializes drivers (\<driver\>_init).
 *       -# Initializes the MMWave module (MMWave_init)
 *       -# Creates/launches the following tasks (the @ref CLI_task is launched indirectly by 
 *          calling @ref CLI_open).
 *     - @ref MmwDemo_mmWaveCtrlTask. This task is used to provide an execution context 
 *        for the mmWave control, it calls in an endless loop the MMWave_execute API.
 *     - @ref MmwDemo_mssCtrlPathTask. The task is used to process data path events coming
 *       from the @ref CLI_task or start/stop events coming from @ref SOC_XWR16XX_GPIO_1 button on 
 *       EVM. It signals the start/stop completion events back to @ref CLI_task.
 *     - @ref CLI_task. This CLI task takes user commands and posts events to the @ref MmwDemo_mssCtrlPathTask.
 *       In case of start/stop commands, it waits for the completion events from @ref MmwDemo_mssCtrlPathTask and
 *       on success, it toggles the LED @ref SOC_XWR16XX_GPIO_2.
 *     - @ref MmwDemo_mboxReadTask. This task handles mailbox messages received from DSS.
 *
 *  @subsection dss_tasks Software Tasks on DSS
 *
 *  The following four (SYSBIOS) tasks are running on DSS:
 *    - @ref MmwDemo_dssInitTask. This task is created/launched by main (in dss_main.c) and is a
 *      one-time active task that performs the following sequence:
 *      -# Initializes drivers (\<driver\>_init).
 *      -# Initializes the MMWave module (MMWave_init)
 *      -# Creates/launches the other three tasks.
 *    - @ref MmwDemo_mboxReadTask. This task handles mailbox messages received from MSS.
 *    - @ref MmwDemo_dssMMWaveCtrlTask. This task is used to provide an execution
 *      context for the mmWave control, it calls in an endless loop the MMWave_execute API.
 *    - @ref MmwDemo_dssDataPathTask. The task performs in real-time:
 *      - Data path processing chain control and (re-)configuration 
 *        of the hardware entities involved in the processing chain, namely EDMA.
 *      - Data path signal processing such as range, Doppler and azimuth DFT,
 *        object detection, and direction of arrival calculation.
 *      - Transfers detected objects to the HS-RAM shared memory and informs MSS that the data
 *        is ready to be sent out to through the UART output port.
 *        For format of the data on UART output port, see @ref MmwDemo_dssSendProcessOutputToMSS.
 *        The UART transmission is done on MSS.
 *
 *      The task pends on the following events:
 *      - @ref MMWDEMO_CONFIG_EVT. This event is posted by @ref MmwDemo_dssMmwaveConfigCallbackFxn
 *        which in called by MMWAVE_ API triggered when MSS issues MMWAVE_config
 *      - @ref MMWDEMO_STOP_EVT. This event is posted by @ref MmwDemo_dssMmwaveStopCallbackFxn when 
 *        @ref MMWave_stop is called from MSS (on CLI sensorStop command). This event causes DSS to go
 *        in @ref MmwDemo_DSS_STATE_STOP_PENDING state. A clock is started for duration equal to 
 *        one frame time. See MMWDEMO_STOP_COMPLETE_EVT event below for more details.
 *      - @ref MMWDEMO_FRAMESTART_EVT. This event originates from BSS firmware
 *        and indicates the beginning of the radar frame. It is posted by interrupt
 *        handler function @ref MmwDemo_dssFrameStartIntHandler.
 *      - @ref MMWDEMO_CHIRP_EVT. This event originates from BSS firmware and
 *        indicates that the ADC buffer, (ping or pong) is filled with ADC samples.
 *        It is posted by @ref MmwDemo_dssChirpIntHandler.
 *      - @ref MMWDEMO_START_EVT. This event is posted by @ref MmwDemo_dssMmwaveStartCallbackFxn when 
 *        @ref MMWave_start is called from MSS (on CLI sensorStart 0 command which means starts with 
 *        no reconfiguration)
 *      - @ref MMWDEMO_STOP_COMPLETE_EVT. This event is posted either when the clock expires (started
 *        by MMWDEMO_STOP_EVT event) or when the ongoing active frame finishes sending data over UART.
 *        This event now moves the DSS to MmwDemo_DSS_STATE_STOP state and executes @ref MmwDemo_dssDataPathStop
 *        which further sends @ref MMWDEMO_DSS2MSS_STOPDONE message back to MSS.
 *
 * 
 *  @section datapath Data Path - Overall
 *   @image html datapath_overall.png "Top Level Data Path Processing Chain"
 *   \n
 *   \n
 *   @image html datapath_overall_timing.png "Top Level Data Path Timing"
 *    
 *   As seen in the above picture, the data path processing consists of:
 *   - Processing during the chirps as seen in the timing diagram. This
 *     consists of
 *     - 1D (range) FFT processing performed by C674x that takes input from
 *       multiple receive antennas from the ADC buffer for every chirp
 *       (corresponding to the chirping pattern on the transmit antennas), and
 *     - transferring transposed output into the L3 RAM by EDMA. More details
 *       can be seen in @ref data1d
 *   - Processing during the idle or cool down period of the RF circuitry following the chirps 
 *     until the next chirping period, shown as "Inter frame processing time" in the timing diagram. 
 *     This processing consists of:
 *     - 2D (velocity) FFT processing performed by C674x that takes input from 1D output in L3 RAM and performs
 *       FFT to give a (range,velocity) matrix in the L3 RAM. The processing also includes the CFAR detection in Doppler direction.
 *       More details can be seen in @ref data2d.
 *     - CFAR detection in range direction using mmWave library.
 *     - Peak Grouping if enabled.
 *     - Direction of Arrival (Azimuth) Estimation. More details can be seen at
 *       @ref dataAngElev and @ref dataXYZ
 *  
 *  @subsection antConfig Antenna Configurations
 *   @image html antenna_layout_simo.png "Single Tx Antenna Configuration"
 *   \n
 *   @image html antenna_layout_mimo.png "TDM-MIMO Antenna Configuration"
 *   \n
 *   As seen in pictures above, the millimeter wave demo supports two antenna configurations:
 *     - Single transmit antenna and four receive antennas.
 *     - Two transmit antennas and four receive antennas. Transmit antennas Tx1 and Tx2 
 *       are horizontally spaced at d = 2 Lambda, with their transmissions interleaved in 
 *       a frame.
 *
 *    Both configurations allow for azimuth estimation.
 *
 *   @subsection data1d Data Path - 1st Dimension FFT Processing
 *   @image html datapath_1d_tdm_mimo.png "Data Path 1D for TDM-MIMO configuration"
 *   \n
 *
 *   Above picture illustrates 1D chirp processing for the case with two
 *   transmit antennas, (TDM-MIMO case), as mentioned in @ref antConfig.
 *   There are 4 rx antennas, the samples of which are color-coded and labelled
 *   as 1,2,3,4 with unique coloring for each of chirps that are processed
 *   in ping-pong manner. The 1D FFT chirp processing is triggered by 
 *   hardware chirp event generated when the ADC
 *   has samples to process in the ADC buffer Ping or Pong memories. The hardware event
 *   triggers the registered chirp event interrupt handler function @ref MmwDemo_dssChirpIntHandler,
 *   that in turn posts @ref MMWDEMO_CHIRP_EVT to @ref MmwDemo_dssDataPathTask. The task initiates EDMA
 *   transfer of rx antenna samples in a ping pong manner to parallelize C674x processing
 *   with EDMA data transfer from ADC buffer to L2 memory.
 *   The processing includes FFT calculation using DSP library function with
 *   16-bit input and output precision. Before FFT calculation, a Blackman window
 *   is applied to ADC samples using mmwlib library function. The calculated 1D
 *   FFT samples are EDMA transferred in transpose manner to the radar cube
 *   matrix in L3 memory. One column of the radar cube matrix contains 1D-FFT
 *   samples of two chirps. Picture below illustrates the shape of radar cube matrix
 *   for one Tx antenna configuration, where one column contains 1D FFT samples of one chirp.
 *
 *   @image html datapath_1d_simo_radar_cube.png "Radar cube matrix for single Tx antenna configuration"
 *   \n
 *
 *   The timing diagram of chirp processing is illustrated in figure below.
 *   @image html datapath_1d_timing.png "Data Path 1D timing diagram"
 *   \n
 *
 *  @subsection data2d Data Path - 2nd Dimension FFT Processing
 *   The 2D processing consists of the following steps:
 *   -# For each range bin it performs:
 *     - 2D-FFT on the samples of 1D-FFT output across chirps,
 *     - log2 magnitude of the output,
 *     - accumulation across all Rx antennas,
 *     - transfer of accumulated values to detection matrix in L3 using EDMA,
 *     - CFAR pre-detection in Doppler direction and saving of Doppler
 *     indices of detected objects for the final CFAR detection in the range direction.
 *   
 *   -# Final CFAR detection in range direction at Doppler indices at which
 *   objects were detected in previous step
 *   -# Peak grouping. Grouping options are specified by CLI CFAR configuration function and can be
 *     - in both range and Doppler direction,
 *     - only in range,
 *     - only in Doppler direction, or
 *     - none.
 *  \n
 *  The 2D processing is shown in figures below.
 *
 *  @image html datapath_2d_detailed_part1.png "2D-FFT Processing - Calculation of Detection Matrix and CFAR in Doppler direction"
 *  \n
 *  \n
 *  @image html datapath_2d_timing.png "2D-FFT Processing - Calculation of Detection Matrix and CFAR in Doppler direction - timing diagram"
 *  \n
 *  \n
 *  @image html datapath_2d_detailed_part2.png "CFAR in range direction and peak grouping"
 *
 *  @subsubsection peakGrouping Peak grouping
 *  Two peak grouping schemes are implemented:
 *  -# Peak grouping based on peaks of the neighboring bins read from detection
 *  matrix. For each CFAR detected peak, listed in @ref MmwDemo_DSS_DataPathObj::detObj2DRaw,
 *  it checks if the peak is greater than its neighbors. If this is true, the
 *  peak is copied to the output list of detected objects
 *  @ref MmwDemo_DSS_DataPathObj::detObj2D. The neighboring peaks that are used
 *  for checking are taken from the detection matrix @ref MmwDemo_DSS_DataPathObj::detMatrix
 *  and are copied into 3x3 kernel regardless of whether they are CFAR detected or not.
 *  -# Peak grouping based on peaks of neighboring bins that are CFAR detected.
 *  For each detected peak the function checks if the peak is greater than its
 *  neighbors. If this is true, the peak is copied to the output list of
 *  detected objects. The neighboring peaks that are used for checking are
 *  taken from the list of CFAR detected objects, (not from the detection matrix),
 *  and are copied into 3x3 kernel that has been initialized to zero for each
 *  peak under test. If the neighboring peak has not been detected by CFAR,
 *  it is not copied into the kernel.
 *
 *  Peak grouping schemes are illustrated in two figures below. The first figure,
 *  illustrating the first scheme, shows how the two targets (out of four) can
 *  be discarded and not presented to the output. For these two targets (at range
 *  indices 3 and 17 in figure below) the CFAR detector did not detect the highest
 *  peak of the target, but only some on the side, and these side peaks are discarded.
 *  The second figure, illustrating the second scheme, shows that all four targets
 *  are presented to the output, one peak per target, with the targets at range
 *  indices 3 and 17 represented with side peaks.
 *
 *  @image html peak_grouping_based_on_detection_matrix.png "Peak grouping based on neighboring peaks from detection matrix"
 *  \n
 *  @image html peak_grouping_based_on_cfar_detected_peaks.png "Peak grouping based on on neighboring CFAR detected peaks"
 *  \n
 *  @subsection dataAngElev Data Path - Direction of Arrival FFT Calculation
 *  Because L3 memory is limited in size, the radar cube matrix stores only the
 *  1D-FFT in 16-bit precision. Because of this, azimuth FFT calculation requires
 *  repeated 2D FFT calculation. Since for each detected object, we need 2D FFT at a
 *  single bin, instead of recalculating 2D-FFT, we calculate single point DFT
 *  at the bin index of each detected object. This calculation is repeated for each received antenna.
 *
 *  Compensation for the Doppler phase shift in the angle estimation is performed on the virtual
 *  antennas (symbols corresponding to the second Tx antenna in case of TDM-MIMO
 *  configuration). These symbols are rotated by half of the estimated Doppler
 *  phase shift between subsequent chirps from the same Tx antenna.
 *  The Doppler shift is calculated using the lookup table @ref MmwDemo_DSS_DataPathObj::azimuthModCoefs
 *  Refer to the pictures below.
 *    @image html angle_doppler_compensation_awr16.png
 *
 *  Currently the size of Azimuth FFT is hardcoded and defined by @ref MMW_NUM_ANGLE_BINS.
 *  The FFT is calculated using DSP lib function DSP_fft32x32. The output of the
 *  function is magnitude squared and the values are stored in floating point format.
 *
 *  @image html datapath_azimuth_fft.png "Direction of arrival calculation"
 *
 *
 *  @subsection dataXYZ Data Path - (X,Y) Estimation
 *    The (x,y) estimation is calculated in the function @ref MmwDemo_XYestimation.
 *    The procedure checks for the second peak in the azimuth FFT, and if
 *    it is detected it appends this additional object to the list.
 *    The procedure consists of the following steps:
 *    -# Find the location <I>k<SUB>MAX</SUB></I> of the peak in azimuth FFT. This will give the estimate of <I>w<SUB>x</SUB></I>.
 *       @image html datapath_azimuth_elevation_eq_wx.png
 *       where <I>N</I> is @ref MMW_NUM_ANGLE_BINS.
 *    -# Calculate range (in meters) as:
 *       @image html datapath_azimuth_elevation_eq_range.png
 *       where, <I>c</I> is the speed of light (m/sec), <I>k<SUB>r</SUB></I> is range index, Fsamp is the sampling frequency (Hz),
 *       <I>S</I> is chirp slope (Hz/sec), <I>N<SUB>FFT</SUB></I> is 1D FFT size.
 *    -# The (x,y) location of the object is computed as:
 *       @image html datapath_azimuth_eq_xy.png
 *    The computed (x,y) and azimuth peak for each object are populated in their
 *    respective positions in @ref MmwDemo_DSS_DataPathObj::detObj2D. Note the azimuth
 *    peak (magnitude squared) replaces the previous CFAR peak (sum of log magnitudes) in the structure.
 *    -# Check for the second peak: in the azimuth FFT, beyond the main peak area,
 *    search for the maximum and compare its height relative to the first peak height,
 *    and if detected, create new object in the list with the same range/Doppler
 *    indices, and repeat steps 2 and 3 to calculate (x/y) coordinates. To enable/disable
 *    the two peak detection or to change the threshold for detection, refer to
 *    @ref MMWDEMO_AZIMUTH_TWO_PEAK_DETECTION_ENABLE and @ref MMWDEMO_AZIMUTH_TWO_PEAK_THRESHOLD_SCALE.
 *
 *  @subsection output Output information sent to host
 *      Output packets with the detection information are sent out every frame
 *      through the UART. Each packet consists of the header @ref MmwDemo_output_message_header
 *      and the number of TLV items containing various data information with
 *      types enumerated in @ref MmwDemo_output_message_type. Each TLV
 *      item consists of type, length (@ref MmwDemo_output_message_tl) and payload information.
 *      The structure of the output packet is illustrated in the following figure.
 *      Since the length of the packet depends on the number of detected objects
 *      it can vary from frame to frame. The end of the packet is padded so that
 *      the total packet length is always multiple of 32 Bytes.
 *
 *      @image html output_packet_uart.png "Output packet structure sent to UART"
 *
 *      The packet construction is initiated on
 *      DSS side at the end of interframe processing. The packet is sent from DSS to MSS via mailbox.
 *      The structure of the packet on the mailbox is illustrated in the following figure. The packet header is followed by the number of TLV elements (@ref MmwDemo_msgTlv)
 *      where the address fields  of these elements point to payloads located eather in HS-RAM or in L3 memory.
 *
 *      @image html output_packet_dss_to_mss.png "Output packet structure sent from DSS to MSS"
 *
 *      The following subsections describe the structure of each TLV.
 *
 *      @subsubsection tlv1 List of detected objects
 *       Type: (@ref MMWDEMO_OUTPUT_MSG_DETECTED_POINTS)
 *
 *       Length: (size of @ref MmwDemo_output_message_dataObjDescr) + (Number of detected objects) x (size of @ref MmwDemo_detectedObj)
 *
 *       Value: List descriptor (@ref MmwDemo_output_message_dataObjDescr) followed by array of detected objects. The information of each detected object
 *       is stored in structure @ref MmwDemo_detectedObj. When the number of detected objects
 *       is zero, this TLV item is not sent. The whole detected objects TLV structure is illustrated in figure below.
 *
 *      @image html detected_objects_tlv.png "Detected objects TLV"
 *
 *      @subsubsection tlv2 Range profile
 *       Type: (@ref MMWDEMO_OUTPUT_MSG_RANGE_PROFILE)
 *
 *       Length: (Range FFT size) x (size of @ref uint16_t)
 *
 *       Value: Array of profile points. In XWR16xx the points represent the sum
 *       of log2 magnitudes of received antennas, expressed in Q8 format.
 *       In XWR14xx the points represent the average of log2 magnitudes of
 *       received antennas, expressed in Q9 format.
 *
 *      @subsubsection tlv3 Noise floor profile
 *       Type: (@ref MMWDEMO_OUTPUT_MSG_NOISE_PROFILE)
 *
 *       Length: (Range FFT size) x (size of @ref uint16_t)
 *
 *       Value: same as range profile
 *
 *      @subsubsection tlv4 Azimuth static heatmap
 *       Type: (@ref MMWDEMO_OUTPUT_MSG_AZIMUT_STATIC_HEAT_MAP)
 *
 *       Length: (Range FFT size) x (Number of virtual antennas) (size of @ref cmplx16ImRe_t)
 *
 *       Value: Array @ref MmwDemo_DSS_DataPathObj::azimuthStaticHeatMap for
 *       construction of the static azimuth heatmap on the host side.
 *
 *      @subsubsection tlv5 Range/Doppler heatmap
 *       Type: (@ref MMWDEMO_OUTPUT_MSG_RANGE_DOPPLER_HEAT_MAP)
 *
 *       Length: (Range FFT size) x (Doppler FFT size) (size of @ref uint16_t)
 *
 *       Value: Detection matrix @ref MmwDemo_DSS_DataPathObj::detMatrix
 *
 *      @subsubsection tlv6 Stats information
 *       Type: (@ref MMWDEMO_OUTPUT_MSG_STATS )
 *
 *       Length: (size of @ref MmwDemo_output_message_stats)
 *
 *       Value: Timing information enclosed in @ref MmwDemo_output_message_stats.
 *      The following figure describes them in the timing diagram.
 *
 *      @image html margins_xwr16xx.png "Margins and DSS CPU loading"
 *
 *  @subsection designNotes Data Path Design Notes
 *    @subsubsection cfar CFAR processing: 
 *      For most scenarios, detection along range dimension is likely to be more difficult (clutter) 
 *      than detection along the doppler dimension. For e.g a simple detection procedure (CFAR-CA) 
 *      might suffice in the doppler dimension, while detection along range dimension 
 *      might require more sophisticated algorithms (e.g. CFAR-OS, histogram based or other heuristics). 
 *      So detection along doppler dimension first might be computationally cheaper:
 *      the first  selection algorithm is less complex, and the subsequent more sophisticated
 *      algorithm runs only on the points detected by the first algorithm. So in this implementation,
 *      we run doppler CFAR first and then run range CFAR on the detected doppler indices. However, 
 *      we currently use the same type of algorithm (CFAR-CA) for the range direction as the doppler direction. 
 *      The range CFAR algorithm could be replaced by a more sophisticated algorithm like CFAR-OS 
 *      to get the benefit of this way of processing.
 *    @subsubsection scaling Scaling
 *      Most of the signal processing in the data path that happens in real-time in 1D and 2D processing
 *      uses fixed-point arithmetic (versus floating point arithmetic). While the C647X is natively capable
 *      of both fixed and floating, the choice here is more for MIPS optimality but using fixed-point requires
 *      some considerations for preventing underflow and overflow while maintaining the desired accuracy
 *      required for correct functionality. 
 *      - 1D processing: If the input to the FFT were a pure tone at one of the bins, then 
 *      the output magnitude of the FFT at that bin will be <CODE> N/2^(ceil(log4(N)-1) (N is the FFT order) </CODE> times
 *      the input tone amplitude (because tone is complex, this implies that the individual real and 
 *      imaginary components will also be amplified by a maximum of this scale). 
 *      Because we do a Hanning window before the FFT, the overall scale is 1/2 of the FFT scale.
 *      This means for example for 256 point FFT, the windowing + FFT scale will be 16. 
 *      Therefore, the ADC output when it is a pure tone should not exceed +/-2^15/16 = 2048 for
 *      the I and Q components. The XWR16xx EVM when presented with a strong single reflector
 *      reasonably close to it (with Rx dB gain of 30 dB in the chirp profile) 
 *      shows ADC samples to be a max of about 3000 and while this exceeds
 *      the 2048 maximum, is not a pure tone, the energy of the FFT is seen in other bins
 *      also and the solution still works well and detects the strong object.
 *      - 2D processing: For the 2D FFT, given that the input is the output of 1D FFT that
 *      can amplify its input as mentioned in previous section, it is more appropriate to use
 *      a 32x32 FFT which helps prevent overflow and reduce quantization noise of the FFT.
 *
 *    @subsubsection edma EDMA versus Cache based Processing
 *      The C647X has L1D cache which is enabled and processing can be done without using EDMA
 *      to access the L2 SRAM and L3 memories through cache. However, the latency to L3 RAM
 *      is much more than L2SRAM and this causes cycle waits that are avoided by using EDMA
 *      to prefetch or postcommit the data into L2SRAM. The L1D is configured part cache (16 KB)
 *      and part SRAM (16 KB). Various buffers involved in data path processing are placed in
 *      L1SRAM and L2SRAM in a way to optimize memory usage by overlaying between and within 1D, 2D
 *      and 3D processing stages. The overlays are documented in the body
 *      of the function @ref MmwDemo_dataPathConfigBuffers and can be disabled using a compile time flag
 *      for debug purposes.
 *      Presently the L3 RAM is not declared as cacheable
 *      (i.e the MAR register settings are defaulted to no caching for L3 RAM).
 *      
 *
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
#include <ti/sysbios/family/arm/v7a/Pmu.h>
#include <ti/sysbios/family/arm/v7r/vim/Hwi.h>

/* mmWave SDK Include Files: */
#include <ti/common/sys_common.h>
#include <ti/drivers/soc/soc.h>
#include <ti/drivers/esm/esm.h>
#include <ti/drivers/crc/crc.h>
#include <ti/drivers/uart/UART.h>
#include <ti/drivers/gpio/gpio.h>
#include <ti/drivers/mailbox/mailbox.h>
#include <ti/control/mmwave/mmwave.h>
#include <ti/utils/cli/cli.h>
#include <ti/drivers/osal/DebugP.h>
#include <ti/drivers/osal/HwiP.h>

#include <ti/drivers/spi/SPI.h>

/* Demo Include Files */
#include "mss_mmw.h"
#include "ti/demo/xwr16xx/mmw/common/mmw_messages.h"




// #define SPI_MULT_ICOUNT_SUPPORT

/**************************************************************************
 *************************** Local Definitions ****************************
 **************************************************************************/

/**************************************************************************
 *************************** Global Definitions ***************************
 **************************************************************************/

/**
 * @brief
 *  Global Variable for tracking information required by the mmw Demo
 */
MmwDemo_MCB gMmwMssMCB;

/**************************************************************************
 *************************** Extern Definitions *******************************
 **************************************************************************/
/* CLI Init function */
extern void MmwDemo_CLIInit(void);

/**************************************************************************
 ************************* Millimeter Wave Demo Functions Prototype**************
 **************************************************************************/

/* Data path functions */
int32_t MmwDemo_mssDataPathConfig(void);
int32_t MmwDemo_mssDataPathStart(void);
int32_t MmwDemo_mssDataPathStop(void);

/* mmwave library call back fundtions */
void MmwDemo_mssMmwaveConfigCallbackFxn(MMWave_CtrlCfg* ptrCtrlCfg);
void MmwDemo_mssMmwaveStartCallbackFxn(void);
void MmwDemo_mssMmwaveStopcallbackFxn(void);
void MmwDemo_mssMmwaveEventCallbackFxn(uint16_t msgId, uint16_t sbId,
                                       uint16_t sbLen, uint8_t *payload);

/* MMW demo Task */
void MmwDemo_mssInitTask(UArg arg0, UArg arg1);
void MmwDemo_mmWaveCtrlTask(UArg arg0, UArg arg1);
void MmwDemo_mssCtrlPathTask(UArg arg0, UArg arg1);
void SPITask(UArg arg0, UArg arg1);

SPI_Handle handle;
bool SPIReadSemaphore = false;
void spiCallback(SPI_Handle handle, SPI_Transaction *transaction);
SPI_Transaction spiTransaction;
Semaphore_Handle SPISem;
DMA_Handle gDmaHandle = NULL;
//char msg[93] = "1abcdefghijklmnopqrstuvwxyzDONE2abcdefghijklmnopqrstuvwxyzDONE3abcdefghijklmnopqrstuvwxyzDONE";
char msg[763] = "Contrary to popular belief, Lorem Ipsum is not simply random text. It has roots in a piece of classical Latin literature from 45 BC, making it over 2000 years old. Richard McClintock, a Latin professor at Hampden-Sydney College in Virginia, looked up one of the more obscure Latin words, consectetur, from a Lorem Ipsum passage, and going through the cites of the word in classical literature, discovered the undoubtable source. Lorem Ipsum comes from sections 1.10.32 and 1.10.33 of de Finibus Bonorum et Malorum (The Extremes of Good and Evil) by Cicero, written in 45 BC. This book is a treatise on the theory of ethics, very popular during the Renaissance. The first line of Lorem Ipsum, Lorem ipsum dolor sit amet.., comes from a line in section 1.10.32.";
char rx[763] = {0};


/**************************************************************************
 ************************* Millimeter Wave Demo Functions **********************
 **************************************************************************/

/**
 *  @b Description
 *  @n
 *      Registered event function which is invoked when an event from the
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
void MmwDemo_mssMmwaveEventCallbackFxn(uint16_t msgId, uint16_t sbId,
                                       uint16_t sbLen, uint8_t *payload)
{
    uint16_t asyncSB = RL_GET_SBID_FROM_UNIQ_SBID(sbId);

#if 0
    System_printf ("Debug: BSS Event MsgId: %d [Sub Block Id: %d Sub Block Length: %d]\n",
            msgId, sbId, sbLen);
#endif

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
            /* Post event to datapath task notify BSS events */
            Event_post(gMmwMssMCB.eventHandle, MMWDEMO_BSS_CPUFAULT_EVT);
            break;
        }
        case RL_RF_AE_ESMFAULT_SB:
        {
            /* Post event to datapath task notify BSS events */
            Event_post(gMmwMssMCB.eventHandle, MMWDEMO_BSS_ESMFAULT_EVT);
            break;
        }
        case RL_RF_AE_INITCALIBSTATUS_SB:
        {
            /* This event should be handled by mmwave internally, ignore the event here */
            break;
        }

        case RL_RF_AE_FRAME_TRIGGER_RDY_SB:
        {
            /* This event is not handled on MSS */
            break;
        }
        case RL_RF_AE_MON_TIMING_FAIL_REPORT_SB:
        {
            /* Increment the statistics for the number of failed reports */
            gMmwMssMCB.stats.numFailedTimingReports++;

#if 0
            /* if something needs to be done then need to implement the function
             to handle the event below in MmwDemo_mssCtrlPathTask()*/

            /* Post event to datapath task notify BSS events */
            Event_post(gMmwMssMCB.eventHandle, MMWDEMO_BSS_MONITORING_REP_EVT);
#endif
            break;
        }
        case RL_RF_AE_RUN_TIME_CALIB_REPORT_SB:
        {
            /* Increment the statistics for the number of received calibration reports */
            gMmwMssMCB.stats.numCalibrationReports++;

#if 0
            /* if something needs to be done then need to implement the function
             to handle the event below in MmwDemo_mssCtrlPathTask()*/

            /* Post event to datapath task notify BSS events */
            Event_post(gMmwMssMCB.eventHandle, MMWDEMO_BSS_CALIBRATION_REP_EVT);
#endif
            break;
        }
        default:
        {
            System_printf("Error: Asynchronous Event SB Id %d not handled\n",
                          asyncSB);
            break;
        }
        }
        break;
    }
    default:
    {
        System_printf("Error: Asynchronous message %d is NOT handled\n", msgId);
        break;
    }
    }

    return;
}

/**
 *  @b Description
 *  @n
 *      Application registered callback function which is invoked after the configuration
 *      has been used to configure the mmWave link and the BSS. This is applicable only for
 *      the XWR16xx. The BSS can be configured only by the MSS *or* DSS. The callback API is
 *      triggered on the remote execution domain (which did not configure the BSS)
 *
 *  @param[in]  ptrCtrlCfg
 *      Pointer to the control configuration
 *
 *  @retval
 *      Not applicable
 */
void MmwDemo_mssMmwaveConfigCallbackFxn(MMWave_CtrlCfg* ptrCtrlCfg)
{
    /* For mmw Demo, mmwave_config() will always be called from MSS, 
     due to the fact CLI is running on MSS, hence this callback won't be called */

    gMmwMssMCB.stats.datapathConfigEvt++;
}

/**
 *  @b Description
 *  @n
 *      Application registered callback function which is invoked the mmWave link on BSS
 *      has been started. This is applicable only for the XWR16xx. The BSS can be configured
 *      only by the MSS *or* DSS. The callback API is triggered on the remote execution
 *      domain (which did not configure the BSS)
 *
 *  @retval
 *      Not applicable
 */
void MmwDemo_mssMmwaveStartCallbackFxn(void)
{
    /* Post an event to main data path task. 
     This function in only called when mmwave_start() is called on DSS */
    gMmwMssMCB.stats.datapathStartEvt++;

    /* Post event to start is done */
    Event_post(gMmwMssMCB.eventHandleNotify, MMWDEMO_DSS_START_COMPLETED_EVT);
}

/**
 *  @b Description
 *  @n
 *      Application registered callback function which is invoked the mmWave link on BSS
 *      has been stopped. This is applicable only for the XWR16xx. The BSS can be configured
 *      only by the MSS *or* DSS. The callback API is triggered on the remote execution
 *      domain (which did not configure the BSS)
 *
 *  @retval
 *      Not applicable
 */
void MmwDemo_mssMmwaveStopCallbackFxn(void)
{
    /* Possible sceanarios:
     1. CLI sensorStop command triggers mmwave_stop() to be called from MSS
     2. In case of Error, mmwave_stop() will be triggered either from MSS or DSS
     */
    gMmwMssMCB.stats.datapathStopEvt++;
}

/**
 *  @b Description
 *  @n
 *      Function to send a message to peer through Mailbox virtural channel 
 *
 *  @param[in]  message
 *      Pointer to the MMW demo message.  
 *
 *  @retval
 *      Success    - 0
 *      Fail       < -1 
 */
int32_t MmwDemo_mboxWrite(MmwDemo_message * message)
{
    int32_t retVal = -1;

    retVal = Mailbox_write(gMmwMssMCB.peerMailbox, (uint8_t*) message,
                           sizeof(MmwDemo_message));
    if (retVal == sizeof(MmwDemo_message))
    {
        retVal = 0;
    }
    return retVal;
}

/**
 *  @b Description
 *  @n
 *      The Task is used to handle  the mmw demo messages received from 
 *      Mailbox virtual channel.  
 *
 *  @param[in]  arg0
 *      arg0 of the Task. Not used
 *  @param[in]  arg1
 *      arg1 of the Task. Not used
 *
 *  @retval
 *      Not Applicable.
 */

void spiCallback(SPI_Handle handle, SPI_Transaction *transaction)
{


   if(transaction->status == SPI_TRANSFER_COMPLETED)
   {
       Semaphore_post(SPISem);

   }

 return;
}

static void MmwDemo_mboxReadTask(UArg arg0, UArg arg1)
{
    MmwDemo_message message;
    int32_t retVal = 0;
    uint32_t totalPacketLen;
    uint32_t numPaddingBytes;
    uint32_t itemIdx;
//    bool val = false;

    /* wait for new message and process all the messsages received from the peer */
    while (1)
    {
        Semaphore_pend(gMmwMssMCB.mboxSemHandle, BIOS_WAIT_FOREVER);

        /* Read the message from the peer mailbox: We are not trying to protect the read
         * from the peer mailbox because this is only being invoked from a single thread */
        retVal = Mailbox_read(gMmwMssMCB.peerMailbox, (uint8_t*) &message,
                              sizeof(MmwDemo_message));
//        if (retVal < 0)
//        {
//            /* Error: Unable to read the message. Setup the error code and return values */
//            System_printf("Error: Mailbox read failed [Error code %d]\n",
//                          retVal);
//        }
//        else if (retVal == 0)
//        {
//            /* We are done: There are no messages available from the peer execution domain. */
//            continue;
//        }
//        else
//        {
//
//
//
//            /* Flush out the contents of the mailbox to indicate that we are done with the message. This will
//             * allow us to receive another message in the mailbox while we process the received message. */
//            Mailbox_readFlush(gMmwMssMCB.peerMailbox);
//
//            /* Process the received message: */
//            switch (message.type)
//            {
//            case MMWDEMO_DSS2MSS_DETOBJ_READY:
//                /* Got detetced objectes , shipped out through UART */
//                /* Send header */
//                totalPacketLen = sizeof(MmwDemo_output_message_header);
//                UART_writePolling(gMmwMssMCB.loggingUartHandle,
//                                  (uint8_t*) &message.body.detObj.header,
//                                  sizeof(MmwDemo_output_message_header));
//
//
//
//
//                /* Send TLVs */
//                for (itemIdx = 0; itemIdx < message.body.detObj.header.numTLVs;
//                        itemIdx++)
//                {
//                    UART_writePolling(
//                            gMmwMssMCB.loggingUartHandle,
//                            (uint8_t*) &message.body.detObj.tlv[itemIdx],
//                            sizeof(MmwDemo_output_message_tl));
//                    UART_writePolling(
//                            gMmwMssMCB.loggingUartHandle,
//                            (uint8_t*) SOC_translateAddress(
//                                    message.body.detObj.tlv[itemIdx].address,
//                                    SOC_TranslateAddr_Dir_FROM_OTHER_CPU, NULL),
//                            message.body.detObj.tlv[itemIdx].length);
//                    totalPacketLen += sizeof(MmwDemo_output_message_tl)
//                            + message.body.detObj.tlv[itemIdx].length;
//                }
//
//                /* Send padding to make total packet length multiple of MMWDEMO_OUTPUT_MSG_SEGMENT_LEN */
//                numPaddingBytes =
//                        MMWDEMO_OUTPUT_MSG_SEGMENT_LEN
//                                - (totalPacketLen
//                                        & (MMWDEMO_OUTPUT_MSG_SEGMENT_LEN - 1));
//                if (numPaddingBytes < MMWDEMO_OUTPUT_MSG_SEGMENT_LEN)
//                {
//                    uint8_t padding[MMWDEMO_OUTPUT_MSG_SEGMENT_LEN];
//                    /*DEBUG:*/memset(&padding, 0xf,
//                                     MMWDEMO_OUTPUT_MSG_SEGMENT_LEN);
//                    UART_writePolling(gMmwMssMCB.loggingUartHandle, padding,
//                                      numPaddingBytes);
//                }
//
//                /* Send a message to MSS to log the output data */
//                memset((void *) &message, 0, sizeof(MmwDemo_message));
//
//                message.type = MMWDEMO_MSS2DSS_DETOBJ_SHIPPED;
//
//                if (MmwDemo_mboxWrite(&message) != 0)
//                {
//                    System_printf("Error: Mailbox send message id=%d failed \n",
//                                  message.type);
//                }
//
//                break;
//            case MMWDEMO_DSS2MSS_STOPDONE:
//                /* Post event that stop is done */
//                Event_post(gMmwMssMCB.eventHandleNotify,
//                           MMWDEMO_DSS_STOP_COMPLETED_EVT);
//                break;
//            default:
//            {
//                /* Message not support */
//                System_printf("Error: unsupport Mailbox message id=%d\n",
//                              message.type);
//                break;
//            }
//            }
//        }

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
void MmwDemo_mboxCallback(Mailbox_Handle handle, Mailbox_Type peer)
{
    /* Message has been received from the peer endpoint. Wakeup the mmWave thread to process
     * the received message. */
    Semaphore_post(gMmwMssMCB.mboxSemHandle);
}

/**
 *  @b Description
 *  @n
 *      Function to do Data Path Configuration on MSS. After received Configuration from
 *    CLI, this function will start the system configuration process, inclucing mmwaveLink, BSS
 *    and DSS.
 *
 *  @retval
 *      0  - Success.
 *      <0 - Failed with errors
 */
int32_t MmwDemo_mssDataPathConfig(void)
{
    int32_t errCode;

    /* Setup the calibration frequency: */
    gMmwMssMCB.cfg.ctrlCfg.freqLimitLow = 760U;
    gMmwMssMCB.cfg.ctrlCfg.freqLimitHigh = 810U;

    /* Configure the mmWave module: */
    if (MMWave_config(gMmwMssMCB.ctrlHandle, &gMmwMssMCB.cfg.ctrlCfg, &errCode)
            < 0)
    {
        System_printf(
                "Error: MMWDemoMSS mmWave Configuration failed [Error code %d]\n",
                errCode);
        return -1;
    }

    return 0;
}

/**
 *  @b Description
 *  @n
 *      Function to do Data Path Start on MSS. After received SensorStart command, MSS will 
 *    start all data path componets including mmwaveLink, BSS and DSS.
 *
 *  @retval
 *      0  - Success.
 *      <0 - Failed with errors
 */
int32_t MmwDemo_mssDataPathStart(void)
{
    int32_t errCode;
    MMWave_CalibrationCfg calibrationCfg;

    /* Initialize the calibration configuration: */
    memset((void*) &calibrationCfg, 0, sizeof(MMWave_CalibrationCfg));

    /* Populate the calibration configuration: */
    calibrationCfg.enableCalibration = true;
    calibrationCfg.enablePeriodicity = true;
    calibrationCfg.periodicTimeInFrames = 10U;

    /* Start the mmWave module: The configuration has been applied successfully. */
    if (MMWave_start(gMmwMssMCB.ctrlHandle, &calibrationCfg, &errCode) < 0)
    {
        /* Error: Unable to start the mmWave control */
        System_printf("Error: MMWDemoMSS mmWave Start failed [Error code %d]\n",
                      errCode);
        return -1;
    }
    System_printf("Debug: MMWDemoMSS mmWave Start succeeded \n");
    return 0;
}

/**
 *  @b Description
 *  @n
 *      Function to do Data Path Start on MSS. After received SensorStart command, MSS will 
 *    start all data path componets including mmwaveLink, BSS and DSS.
 *
 *  @retval
 *      0  - Success.
 *      <0 - Failed with errors
 */
int32_t MmwDemo_mssDataPathStop(void)
{
    int32_t errCode;

    /* Start the mmWave module: The configuration has been applied successfully. */
    if (MMWave_stop(gMmwMssMCB.ctrlHandle, &errCode) < 0)
    {
        /* Error: Unable to start the mmWave control */
        System_printf("Error: MMWDemoMSS mmWave Stop failed [Error code %d]\n",
                      errCode);
        return -1;
    }

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
void MmwDemo_mmWaveCtrlTask(UArg arg0, UArg arg1)
{
    int32_t errCode;

    while (1)
    {
        /* Execute the mmWave control module: */
        if (MMWave_execute(gMmwMssMCB.ctrlHandle, &errCode) < 0)
            System_printf(
                    "Error: mmWave control execution failed [Error code %d]\n",
                    errCode);
    }
}

/**
 *  @b Description
 *  @n
 *      This is a utility function which can be invoked from the CLI or
 *      the Switch press to start the sensor. This sends an event to the
 *      sensor management task where the actual *start* procedure is
 *      implemented.
 *
 *  @retval
 *      Not applicable
 */
void MmwDemo_notifySensorStart(bool doReconfig)
{
    gMmwMssMCB.stats.cliSensorStartEvt++;

    if (doReconfig)
    {
        /* Get the configuration from the CLI mmWave Extension */
        CLI_getMMWaveExtensionConfig(&gMmwMssMCB.cfg.ctrlCfg);

        /* Post sensorStart event to start reconfig and then start the sensor */
        Event_post(gMmwMssMCB.eventHandle, MMWDEMO_CLI_SENSORSTART_EVT);
    }
    else
    {
        /* Post frameStart event to skip reconfig and just start the sensor */
        Event_post(gMmwMssMCB.eventHandle, MMWDEMO_CLI_FRAMESTART_EVT);
    }
}

/**
 *  @b Description
 *  @n
 *      This is a utility function which can be invoked from the CLI or
 *      the Switch press to start the sensor. This sends an event to the
 *      sensor management task where the actual *start* procedure is
 *      implemented.
 *
 *  @retval
 *      Not applicable
 */
void MmwDemo_notifySensorStop(void)
{
    gMmwMssMCB.stats.cliSensorStopEvt++;

    /* Post sensorSTOP event to notify sensor stop command */
    Event_post(gMmwMssMCB.eventHandle, MMWDEMO_CLI_SENSORSTOP_EVT);
}

/**
 *  @b Description
 *  @n
 *      This is a utility function which can be used by the CLI to 
 *      pend for start complete (after MmwDemo_notifySensorStart is called)
 *
 *  @retval
 *      Not applicable
 */
int32_t MmwDemo_waitSensorStartComplete(void)
{
    UInt event;
    int32_t retVal;
    /* Pend on the START NOTIFY event */
    event = Event_pend(
            gMmwMssMCB.eventHandleNotify,
            Event_Id_NONE,
            MMWDEMO_DSS_START_COMPLETED_EVT | MMWDEMO_DSS_START_FAILED_EVT,
            BIOS_WAIT_FOREVER);

    /************************************************************************
     * DSS event:: START notification
     ************************************************************************/
    if (event & MMWDEMO_DSS_START_COMPLETED_EVT)
    {
        /* Sensor has been started successfully */
        gMmwMssMCB.isSensorStarted = true;
        /* Turn on the LED */
        GPIO_write(SOC_XWR16XX_GPIO_2, 1U);
        retVal = 0;
    }
    else if (event & MMWDEMO_DSS_START_FAILED_EVT)
    {
        /* Sensor start failed */
        gMmwMssMCB.isSensorStarted = false;
        retVal = -1;
    }
    else
    {
        /* we should block forever till we get the events. If the desired event 
         didnt happen, then throw an assert */
        retVal = -1;
        DebugP_assert(0);
    }
    return retVal;
}

/**
 *  @b Description
 *  @n
 *      This is a utility function which can be used by the CLI to 
 *      pend for stop complete (after MmwDemo_notifySensorStart is called)
 *
 *  @retval
 *      Not applicable
 */
void MmwDemo_waitSensorStopComplete(void)
{
    UInt event;
    /* Pend on the START NOTIFY event */
    event = Event_pend(gMmwMssMCB.eventHandleNotify,
    Event_Id_NONE,
                       MMWDEMO_DSS_STOP_COMPLETED_EVT,
                       BIOS_WAIT_FOREVER);

    /************************************************************************
     * DSS event:: STOP notification
     ************************************************************************/
    if (event & MMWDEMO_DSS_STOP_COMPLETED_EVT)
    {
        /* Sensor has been stopped successfully */
        gMmwMssMCB.isSensorStarted = false;

        /* Turn off the LED */
        GPIO_write(SOC_XWR16XX_GPIO_2, 0U);

        /* print for user */
        System_printf("Sensor has been stopped\n");
    }
    else
    {
        /* we should block forever till we get the event. If the desired event 
         didnt happen, then throw an assert */
        DebugP_assert(0);
    }
}

/**
 *  @b Description
 *  @n
 *      Callback function invoked when the GPIO switch is pressed.
 *      This is invoked from interrupt context.
 *
 *  @param[in]  index
 *      GPIO index configured as input
 *
 *  @retval
 *      Not applicable
 */
static void MmwDemo_switchPressFxn(unsigned int index)
{
    /* Was the sensor started? */
    if (gMmwMssMCB.isSensorStarted == true)
    {
        /* YES: We need to stop the sensor now */
        MmwDemo_notifySensorStop();
    }
    else
    {
        /* NO: We need to start the sensor now. */
        MmwDemo_notifySensorStart(true);
    }
}

/**
 *  @b Description
 *  @n
 *      The task is used to process data path events
 *      control task
 *
 *  @retval
 *      Not Applicable.
 */

void SPITask(UArg arg0, UArg arg1)
{


    System_printf("SPI Task Started \n");

    spiTransaction.count = (size_t)700;
    spiTransaction.txBuf = (void*) msg;
    spiTransaction.rxBuf = (void*)rx;
    spiTransaction.arg = NULL;
    spiTransaction.slaveIndex = 0U;

    while(1){
        Semaphore_pend(SPISem, BIOS_WAIT_FOREVER);
        System_printf("Transfer now\n");
        SPI_transfer(handle, &spiTransaction);
        //System_printf("Transfer now\n");
        Semaphore_post(SPISem);
    }



}


void MmwDemo_mssCtrlPathTask(UArg arg0, UArg arg1)
{
    UInt event;

    /**********************************************************************
     * Setup the PINMUX:
     * - GPIO Input : Configure pin J13 as GPIO_1 input
     * - GPIO Output: Configure pin K13 as GPIO_2 output
     **********************************************************************/
    Pinmux_Set_OverrideCtrl(SOC_XWR16XX_PINJ13_PADAC,
                            PINMUX_OUTEN_RETAIN_HW_CTRL,
                            PINMUX_INPEN_RETAIN_HW_CTRL);
    Pinmux_Set_FuncSel(SOC_XWR16XX_PINJ13_PADAC,
                       SOC_XWR16XX_PINJ13_PADAC_GPIO_1);
    Pinmux_Set_OverrideCtrl(SOC_XWR16XX_PINK13_PADAZ,
                            PINMUX_OUTEN_RETAIN_HW_CTRL,
                            PINMUX_INPEN_RETAIN_HW_CTRL);
    Pinmux_Set_FuncSel(SOC_XWR16XX_PINK13_PADAZ,
                       SOC_XWR16XX_PINK13_PADAZ_GPIO_2);

    /**********************************************************************
     * Setup the SW1 switch on the EVM connected to GPIO_1
     * - This is used as an input
     * - Enable interrupt to be notified on a switch press
     **********************************************************************/
    GPIO_setConfig(
            SOC_XWR16XX_GPIO_1,
            GPIO_CFG_INPUT | GPIO_CFG_IN_INT_RISING | GPIO_CFG_IN_INT_LOW);
    GPIO_setCallback(SOC_XWR16XX_GPIO_1, MmwDemo_switchPressFxn);
    GPIO_enableInt(SOC_XWR16XX_GPIO_1);

    /**********************************************************************
     * Setup the DS3 LED on the EVM connected to GPIO_2
     **********************************************************************/
    GPIO_setConfig(SOC_XWR16XX_GPIO_2, GPIO_CFG_OUTPUT);

    /* Data Path management task Main loop */
    while (1)
    {
        event = Event_pend(gMmwMssMCB.eventHandle,
        Event_Id_NONE,
                           MMWDEMO_CLI_EVENTS | MMWDEMO_BSS_FAULT_EVENTS,
                           BIOS_WAIT_FOREVER);

        /************************************************************************
         * CLI event:: SensorStart
         ************************************************************************/

        if (event & MMWDEMO_CLI_SENSORSTART_EVT)
        {
            System_printf("Debug: MMWDemoMSS Received CLI sensorStart Event\n");

            /* Setup the data path: */
            if (gMmwMssMCB.isSensorStarted == false)
            {
                if (MmwDemo_mssDataPathConfig() < 0)
                {
                    /* Post start failed event; error printing is done in function above */
                    Event_post(gMmwMssMCB.eventHandleNotify,
                               MMWDEMO_DSS_START_FAILED_EVT);
                    continue;
                }
                else
                {
                    /* start complete event is posted via DSS */
                }
            }
            else
            {
                /* Post start complete event as this is just a duplicate command */
                Event_post(gMmwMssMCB.eventHandleNotify,
                           MMWDEMO_DSS_START_COMPLETED_EVT);
                continue;
            }
        }

        /************************************************************************
         * CLI event:: SensorStop
         ************************************************************************/
        if (event & MMWDEMO_CLI_SENSORSTOP_EVT)
        {
            if (gMmwMssMCB.isSensorStarted == true)
            {
                if (MmwDemo_mssDataPathStop() < 0)
                {
                    /* do we need stop fail event; for now just mark the stop as completed */
                    Event_post(gMmwMssMCB.eventHandleNotify,
                               MMWDEMO_DSS_STOP_COMPLETED_EVT);
                    continue;
                }
                else
                {
                    /* DSS will post the stop completed event */
                }
            }
            else
            {
                /* Post stop complete event as this is just a duplicate command */
                Event_post(gMmwMssMCB.eventHandleNotify,
                           MMWDEMO_DSS_STOP_COMPLETED_EVT);
                continue;
            }
        }

        /************************************************************************
         * CLI event:: Framestart
         ************************************************************************/
        if (event & MMWDEMO_CLI_FRAMESTART_EVT)
        {
            /* error printing happens inside this function */
            if (gMmwMssMCB.isSensorStarted == false)
            {
                if (MmwDemo_mssDataPathStart() < 0)
                {
                    /* Post start failed event; error printing is done in function above */
                    Event_post(gMmwMssMCB.eventHandleNotify,
                               MMWDEMO_DSS_START_FAILED_EVT);
                    continue;
                }
            }

            /* Post event to start is done */
            Event_post(gMmwMssMCB.eventHandleNotify,
                       MMWDEMO_DSS_START_COMPLETED_EVT);
        }

        /************************************************************************
         * BSS event:: CPU fault
         ************************************************************************/
        if (event & MMWDEMO_BSS_CPUFAULT_EVT)
        {
            DebugP_assert(0);
            break;
        }

        /************************************************************************
         * BSS event:: ESM fault
         ************************************************************************/
        if (event & MMWDEMO_BSS_ESMFAULT_EVT)
        {
            DebugP_assert(0);
            break;
        }

    }

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
void MmwDemo_mssInitTask(UArg arg0, UArg arg1)
{
    int32_t errCode;
    MMWave_InitCfg initCfg;
    //UART_Params uartParams;
    Task_Params taskParams;
    Task_Params spiTaskParams;
    Semaphore_Params semParams;
    Mailbox_Config mboxCfg;
    Error_Block eb;
    SPI_Params params;
    DMA_Params dmaParams;





//      bool val = false;


    /* Debug Message: */
    System_printf("Debug: MMWDemoMSS Launched the Initialization Task\n");

    /*****************************************************************************
     * Initialize the mmWave SDK components:
     ***********************************************************an******************/
    /* Pinmux setting */

    /* Setup the PINMUX to bring out the UART-1 */
    Pinmux_Set_OverrideCtrl(SOC_XWR16XX_PINN5_PADBE,
                            PINMUX_OUTEN_RETAIN_HW_CTRL,
                            PINMUX_INPEN_RETAIN_HW_CTRL);
    Pinmux_Set_FuncSel(SOC_XWR16XX_PINN5_PADBE,
                       SOC_XWR16XX_PINN5_PADBE_MSS_UARTA_TX);
    Pinmux_Set_OverrideCtrl(SOC_XWR16XX_PINN4_PADBD,
                            PINMUX_OUTEN_RETAIN_HW_CTRL,
                            PINMUX_INPEN_RETAIN_HW_CTRL);
    Pinmux_Set_FuncSel(SOC_XWR16XX_PINN4_PADBD,
                       SOC_XWR16XX_PINN4_PADBD_MSS_UARTA_RX);

    /* Setup the PINMUX to bring out the UART-3 */
    Pinmux_Set_OverrideCtrl(SOC_XWR16XX_PINF14_PADAJ,
                            PINMUX_OUTEN_RETAIN_HW_CTRL,
                            PINMUX_INPEN_RETAIN_HW_CTRL);
    Pinmux_Set_FuncSel(SOC_XWR16XX_PINF14_PADAJ,
                       SOC_XWR16XX_PINF14_PADAJ_MSS_UARTB_TX);

    /* Setup the PINMUX to bring out the DSS UART */
    Pinmux_Set_OverrideCtrl(SOC_XWR16XX_PINP8_PADBM,
                            PINMUX_OUTEN_RETAIN_HW_CTRL,
                            PINMUX_INPEN_RETAIN_HW_CTRL);
    Pinmux_Set_FuncSel(SOC_XWR16XX_PINP8_PADBM,
                       SOC_XWR16XX_PINP8_PADBM_DSS_UART_TX);

    /*=======================================
     * Setup the PINMUX to bring out the MibSpiA
     *=======================================*/
    /* NOTE: Please change the following pin configuration according
     to EVM used for the test */

    /* SPIA_MOSI */
     Pinmux_Set_OverrideCtrl(SOC_XWR16XX_PIND13_PADAD, PINMUX_OUTEN_RETAIN_HW_CTRL, PINMUX_INPEN_RETAIN_HW_CTRL);
     Pinmux_Set_FuncSel(SOC_XWR16XX_PIND13_PADAD, SOC_XWR16XX_PIND13_PADAD_SPIA_MOSI);

     /* SPIA_MISO */
     Pinmux_Set_OverrideCtrl(SOC_XWR16XX_PINE14_PADAE, PINMUX_OUTEN_RETAIN_HW_CTRL, PINMUX_INPEN_RETAIN_HW_CTRL);
     Pinmux_Set_FuncSel(SOC_XWR16XX_PINE14_PADAE, SOC_XWR16XX_PINE14_PADAE_SPIA_MISO);

     /* SPIA_CLK */
     Pinmux_Set_OverrideCtrl(SOC_XWR16XX_PINE13_PADAF, PINMUX_OUTEN_RETAIN_HW_CTRL, PINMUX_INPEN_RETAIN_HW_CTRL);
     Pinmux_Set_FuncSel(SOC_XWR16XX_PINE13_PADAF, SOC_XWR16XX_PINE13_PADAF_SPIA_CLK);

     /* SPIA_CS */
     Pinmux_Set_OverrideCtrl(SOC_XWR16XX_PINC13_PADAG, PINMUX_OUTEN_RETAIN_HW_CTRL, PINMUX_INPEN_RETAIN_HW_CTRL);
     Pinmux_Set_FuncSel(SOC_XWR16XX_PINC13_PADAG, SOC_XWR16XX_PINC13_PADAG_SPIA_CSN);


     DMA_init();

    /* Initialize the UART */
    //UART_init();

    /* Initialize the GPIO */
    GPIO_init();

    /* Initialize the SPI */
    SPI_init();





    /* Initialize the Mailbox */
    Mailbox_init(MAILBOX_TYPE_MSS);

    /*****************************************************************************
     * Open & configure the drivers:
     *****************************************************************************/

    /* Setup the default UART Parameters */
//    UART_Params_init(&uartParams);
//    uartParams.clockFrequency = gMmwMssMCB.cfg.sysClockFrequency;
//    uartParams.baudRate = gMmwMssMCB.cfg.commandBaudRate;
//    uartParams.isPinMuxDone = 1U;
//
//    /* Open the UART Instance */
//    gMmwMssMCB.commandUartHandle = UART_open(0, &uartParams);
//    if (gMmwMssMCB.commandUartHandle == NULL)
//    {
//        System_printf(
//                "Error: MMWDemoMSS Unable to open the Command UART Instance\n");
//        return;
//    }
//
//    /* Setup the default UART Parameters */
//    UART_Params_init(&uartParams);
//    uartParams.writeDataMode = UART_DATA_BINARY;
//    uartParams.readDataMode = UART_DATA_BINARY;
//    uartParams.clockFrequency = gMmwMssMCB.cfg.sysClockFrequency;
//    uartParams.baudRate = gMmwMssMCB.cfg.loggingBaudRate;
//    uartParams.isPinMuxDone = 1U;
//
//    /* Open the Logging UART Instance: */
//    gMmwMssMCB.loggingUartHandle = UART_open(1, &uartParams);
//    if (gMmwMssMCB.loggingUartHandle == NULL)
//    {
//        System_printf(
//                "Error: MMWDemoMSS Unable to open the Logging UART Instance\n");
//        return;
//    }



    DMA_Params_init(&dmaParams);
    gDmaHandle = DMA_open(0, &dmaParams, &errCode);
    if(gDmaHandle == NULL)
    {
        printf("Open DMA driver failed with error=%d\n", errCode);
        return;
    }


    SPI_Params_init(&params);
    params.mode = SPI_SLAVE;

    params.dataSize = 8;
    params.dmaEnable = 1;
    params.dmaHandle = gDmaHandle;
    params.u.slaveParams.dmaCfg.txDmaChanNum =1U;
    params.u.slaveParams.dmaCfg.rxDmaChanNum =0U;
    params.frameFormat = SPI_POL0_PHA1;
    params.shiftFormat = SPI_MSB_FIRST;
    params.pinMode = SPI_PINMODE_4PIN_CS;
    params.transferMode = SPI_MODE_BLOCKING;
    params.eccEnable = 1;
    params.csHold = 1;
    //params.transferCallbackFxn = spiCallback;

    handle = SPI_open(0, &params);
    if (!handle)
    {
        System_printf("SPI did not open \n");
    }

//        val = SPI_transfer(handle, &spiTransaction);
//        if (val)
//        {
//            System_printf("Sent \n");
//        }


    /*****************************************************************************
     * Creating communication channel between MSS & DSS
     *****************************************************************************/

    /* Create a binary semaphore which is used to handle mailbox interrupt. */
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    gMmwMssMCB.mboxSemHandle = Semaphore_create(0, &semParams, NULL);


    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    SPISem = Semaphore_create(0, &semParams, NULL);




    /* Setup the default mailbox configuration */
    Mailbox_Config_init(&mboxCfg);

    /* Setup the configuration: */
    mboxCfg.chType = MAILBOX_CHTYPE_MULTI;
    mboxCfg.chId = MAILBOX_CH_ID_0;
    mboxCfg.writeMode = MAILBOX_MODE_BLOCKING;
    mboxCfg.readMode = MAILBOX_MODE_CALLBACK;
    mboxCfg.readCallback = &MmwDemo_mboxCallback;

    /* Initialization of Mailbox Virtual Channel  */
    gMmwMssMCB.peerMailbox = Mailbox_open(MAILBOX_TYPE_DSS, &mboxCfg, &errCode);
    if (gMmwMssMCB.peerMailbox == NULL)
    {
        /* Error: Unable to open the mailbox */
        System_printf(
                "Error: Unable to open the Mailbox to the DSS [Error code %d]\n",
                errCode);
        return;
    }

    /* Create task to handle mailbox messges */
    Task_Params_init(&taskParams);
    taskParams.stackSize = 12 * 1024;
    Task_create(MmwDemo_mboxReadTask, &taskParams, NULL);

    /*****************************************************************************
     * Create Event to handle mmwave callback and system datapath events 
     *****************************************************************************/
    /* Default instance configuration params */
    Error_init(&eb);
    gMmwMssMCB.eventHandle = Event_create(NULL, &eb);
    if (gMmwMssMCB.eventHandle == NULL)
    {
        DebugP_assert(0);
        return;
    }

    Error_init(&eb);
    gMmwMssMCB.eventHandleNotify = Event_create(NULL, &eb);
    if (gMmwMssMCB.eventHandleNotify == NULL)
    {
        DebugP_assert(0);
        return;
    }

    /*****************************************************************************
     * mmWave: Initialization of the high level module
     *****************************************************************************/

    /* Initialize the mmWave control init configuration */
    memset((void*) &initCfg, 0, sizeof(MMWave_InitCfg));

    /* Populate the init configuration for mmwave library: */
    initCfg.domain = MMWave_Domain_MSS;
    initCfg.socHandle = gMmwMssMCB.socHandle;
    initCfg.eventFxn = MmwDemo_mssMmwaveEventCallbackFxn;
    initCfg.linkCRCCfg.useCRCDriver = 1U;
    initCfg.linkCRCCfg.crcChannel = CRC_Channel_CH1;
    initCfg.cfgMode = MMWave_ConfigurationMode_FULL;
    initCfg.executionMode = MMWave_ExecutionMode_COOPERATIVE;
    initCfg.cooperativeModeCfg.cfgFxn = MmwDemo_mssMmwaveConfigCallbackFxn;
    initCfg.cooperativeModeCfg.startFxn = MmwDemo_mssMmwaveStartCallbackFxn;
    initCfg.cooperativeModeCfg.stopFxn = MmwDemo_mssMmwaveStopCallbackFxn;

    /* Initialize and setup the mmWave Control module */
    gMmwMssMCB.ctrlHandle = MMWave_init(&initCfg, &errCode);
    if (gMmwMssMCB.ctrlHandle == NULL)
    {
        /* Error: Unable to initialize the mmWave control module */
        System_printf(
                "Error: MMWDemoMSS mmWave Control Initialization failed [Error code %d]\n",
                errCode);
        return;
    }
    System_printf(
            "Debug: MMWDemoMSS mmWave Control Initialization was successful\n");

    /* Synchronization: This will synchronize the execution of the control module
     * between the domains. This is a prerequiste and always needs to be invoked. */
    while (1)
    {
        int32_t syncStatus;

        /* Get the synchronization status: */
        syncStatus = MMWave_sync(gMmwMssMCB.ctrlHandle, &errCode);
        if (syncStatus < 0)
        {
            /* Error: Unable to synchronize the mmWave control module */
            System_printf(
                    "Error: MMWDemoMSS mmWave Control Synchronization failed [Error code %d]\n",
                    errCode);
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
    taskParams.stackSize = 3 * 1024;
    Task_create(MmwDemo_mmWaveCtrlTask, &taskParams, NULL);

    /*****************************************************************************
     * Create a data path management task to handle data Path events
     *****************************************************************************/
//    Task_Params_init(&taskParams);
//    taskParams.priority = 4;
//    taskParams.stackSize = 3 * 1024;
//    Task_create(MmwDemo_mssCtrlPathTask, &taskParams, NULL);

    Task_Params_init(&spiTaskParams);
    spiTaskParams.priority = 4;
    spiTaskParams.stackSize =  3*1024;
    Task_create(SPITask,&spiTaskParams, NULL);

    Semaphore_post(SPISem);

    /*****************************************************************************
     * At this point, MSS and DSS are both up and synced. Configuration is ready to be sent.
     * Start CLI to get configuration from user
     *****************************************************************************/
    //MmwDemo_CLIInit();

    /*****************************************************************************
     * Benchmarking Count init
     *****************************************************************************/
    /* Configure benchmark counter */
    Pmu_configureCounter(0, 0x11, FALSE);
    Pmu_startCounter(0);

    return;
}

/**
 *  @b Description
 *  @n
 *      Entry point into the Millimeter Wave Demo
 *
 *  @retval
 *      Not Applicable.
 */
int main(void)
{
    Task_Params taskParams;
//    Task_Params spiTaskParams;
    int32_t errCode;
    SOC_Cfg socCfg;

    /* Initialize the ESM: */
    ESM_init(0U); //dont clear errors as TI RTOS does it

    /* Initialize and populate the demo MCB */
    memset((void*) &gMmwMssMCB, 0, sizeof(MmwDemo_MCB));

    /* Initialize the SOC confiugration: */
    memset((void *) &socCfg, 0, sizeof(SOC_Cfg));

    /* Populate the SOC configuration: */
    socCfg.clockCfg = SOC_SysClock_INIT;

    /* Initialize the SOC Module: This is done as soon as the application is started
     * to ensure that the MPU is correctly configured. */
    gMmwMssMCB.socHandle = SOC_init(&socCfg, &errCode);
    if (gMmwMssMCB.socHandle == NULL)
    {
        System_printf(
                "Error: SOC Module Initialization failed [Error code %d]\n",
                errCode);
        return -1;
    }

    /* Initialize the DEMO configuration: */
    gMmwMssMCB.cfg.sysClockFrequency = MSS_SYS_VCLK;
    gMmwMssMCB.cfg.loggingBaudRate = 921600;
    gMmwMssMCB.cfg.commandBaudRate = 115200;

    /* Debug Message: */
    System_printf("**********************************************\n");
    System_printf("Debug: Launching the Millimeter Wave Demo\n");
    System_printf("**********************************************\n");

    /* Initialize the Task Parameters. */
    Task_Params_init(&taskParams);
    taskParams.priority = 3;
    Task_create(MmwDemo_mssInitTask, &taskParams, NULL);

    /* Start BIOS */
    BIOS_start();
    return 0;
}
