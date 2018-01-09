 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
 %
 %      (C) Copyright 2016 Texas Instruments, Inc.
 %
 %  Redistribution and use in source and binary forms, with or without
 %  modification, are permitted provided that the following conditions
 %  are met:
 %
 %    Redistributions of source code must retain the above copyright
 %    notice, this list of conditions and the following disclaimer.
 %
 %    Redistributions in binary form must reproduce the above copyright
 %    notice, this list of conditions and the following disclaimer in the
 %    documentation and/or other materials provided with the
 %    distribution.
 %
 %    Neither the name of Texas Instruments Incorporated nor the names of
 %    its contributors may be used to endorse or promote products derived
 %    from this software without specific prior written permission.
 %
 %  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 %  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 %  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 %  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 %  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 %  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 %  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 %  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 %  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 %  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 %  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 %
 %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%platform    : supported platforms: xwr14xx or xwr16xx
%comportSnum : comport on which visualization data is being sent
%range_depth (in meters) : determines the y-axis of the plot
%range_width (in meters) : determines the x-axis (one sided) of the plot
%comportCliNum : comport over which the cli configuration is sent to XWR16xx
%cliCfgFileName : Input cli configuration file
%loadCfg : loadCfg=1: cli configuration is sent to XWR16xx, loadCfg=0: it is
%          assumed that configuration is already sent to XWR16xx, and it is
%          already running, the configuration is not sent to XWR16xx, but it
%          will keep receiving and displaying incomming data.
%maxRangeProfileYaxis : maximum range profile y-axis
%%%%%%%%%%%EXPECTED FORMAT OF DATA BEING RECEIVED%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%Magic Number : [2 1 4 3  6 5  8 7] : 8 bytes
%Inter frame processing time in CPU cycles : 4 bytes 
%Noise Energy :  4 bytes (uint32)
%Number of valid objects : 4 bytes
%The following data are sent depending on the cli configuration
%Object Data :  10 bytes per object *MAX_NUM_OBJECTS = 500 bytes [see structure of object data below objOut_t]
%Range Profile : 1DfftSize * sizeof(uint16_t) bytes
%2D range bins at zero Doppler, complex symbols including all received
%virtual antennas : 1DfftSize * sizeof(uint32_t) * numVirtualAntennas
%Doppler-Range 2D FFT log magnitude matrix: 1DfftSize * 2Dfftsize * sizeof(uint16_t)
%
% typedef volatile struct objOut
% {
%     uint8_t   rangeIdx;
%     uint8_t   dopplerIdx ;
%     uint16_t  peakVal;
%     int16_t  x; //in meters
%     int16_t  y; //in meters
%     int16_t  z; //in meters
% } objOut_t;
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function [] = mmw_demo(platform, comportSnum, range_depth, range_width, comportCliNum, cliCfgFileName, loadCfg, maxRangeProfileYaxis, debugFlag)

fprintf('Starting UI for mmWave Demo ....\n'); 
save_flag=0;
log_idx=0;
if (nargin < 6)
    fprintf('!!ERROR:Missing arguments!!\n');
    fprintf('Specify arguments in this order: <platform> <comportSnum> <range_depth> <range_width> <comportCliNum> <cliCfgFileName> <loadCfg> \n');
    fprintf('platform                : ''xwr14xx'' or ''xwr16xx'' \n');
    fprintf('comportSnum             : comport on which visualization data is being sent \n');
    fprintf('range_depth (in meters) : determines the y-axis of the plot \n');
    fprintf('range_width (in meters) : determines the x-axis (one sided) of the plot \n');
    fprintf('comportCliNum           : comport over which the cli configuration is sent to XWR16xx \n');
    fprintf('cliCfgFileName          : Input cli configuration file \n');
    fprintf('loadCfg                 : loadCfg=1: cli configuration is sent to XWR16xx \n');
    fprintf('                          loadCfg=0: it is assumed that configuration is already sent to XWR16xx\n');
    fprintf('                                     and it is already running, the configuration is not sent to XWR16xx, \n');
    fprintf('                                     but it will keep receiving and displaying incomming data. \n');
    
    return;
end
if(ischar(comportSnum))
    comportSnum=str2num(comportSnum);
    range_depth=str2num(range_depth);
    range_width=str2num(range_width);
end

if (range_depth/4 <=2)
    range_grid_arc = .5;
elseif (range_depth <=2)
    range_grid_arc = .5;
elseif (range_depth <=4)
    range_grid_arc = 1;
elseif (range_depth <=20)
    range_grid_arc = 5;
elseif (range_depth <=24)
    range_grid_arc = 10;
else
    range_grid_arc = 15;
end

if exist('maxRangeProfileYaxis')
    if(ischar(maxRangeProfileYaxis))
        maxRangeProfileYaxis = str2num(maxRangeProfileYaxis);
    end
else
    maxRangeProfileYaxis = 1e6;
end


if exist('debugFlag')
    if strfind(debugFlag, 'debugFlag')
         debugFlag = 1;
    else
        debugFlag = 0;
    end
else
    debugFlag = 0;
end

global platformType
if exist('platform')
    if strfind(platform, 'xwr16xx')
         platformType = hex2dec('a1642');
    elseif strfind(platform, 'xwr14xx')
        platformType = hex2dec('a1443');
    else
        fprintf('Unknown platform \n');
        return
    end
else
    platformType = hex2dec('a1642');
end

if ischar(loadCfg)
    loadCfg = str2num(loadCfg);
end

global MAX_NUM_OBJECTS;
global OBJ_STRUCT_SIZE_BYTES ;
global TOTAL_PAYLOAD_SIZE_BYTES;

MMWDEMO_UART_MSG_DETECTED_POINTS = 1;
MMWDEMO_UART_MSG_RANGE_PROFILE   = 2;
MMWDEMO_UART_MSG_NOISE_PROFILE   = 3;
MMWDEMO_UART_MSG_AZIMUT_STATIC_HEAT_MAP = 4;
MMWDEMO_UART_MSG_RANGE_DOPPLER_HEAT_MAP = 5;
MMWDEMO_UART_MSG_STATS =6;


%display('version 0.6');

% below defines correspond to the mmw demo code
MAX_NUM_OBJECTS = 100;
OBJ_STRUCT_SIZE_BYTES = 12;
NUM_ANGLE_BINS = 64;

global STATS_SIZE_BYTES
STATS_SIZE_BYTES = 16;


global bytevec_log;
bytevec_log = [];

global readUartFcnCntr;
readUartFcnCntr = 0;

global ELEV_VIEW 
ELEV_VIEW = 3;
global EXIT_KEY_PRESSED
EXIT_KEY_PRESSED = 0;

global Params
global hndChirpParamTable
hndChirpParamTable = 0;

global BYTE_VEC_ACC_MAX_SIZE
BYTE_VEC_ACC_MAX_SIZE = 2^15;
global bytevecAcc
bytevecAcc = zeros(BYTE_VEC_ACC_MAX_SIZE,1);
global bytevecAccLen
bytevecAccLen = 0;


global BYTES_AVAILABLE_FLAG
BYTES_AVAILABLE_FLAG = 0;

global BYTES_AVAILABLE_FCN_CNT
BYTES_AVAILABLE_FCN_CNT = 32*8;

    
%Setup the main figure
figHnd = figure(1);
clf(figHnd);
if platformType == hex2dec('a1642')
    set(figHnd,'Name','Texas Instruments - XWR16xx mmWave Demo Visualization','NumberTitle','off')
elseif platformType == hex2dec('a1443')
    set(figHnd,'Name','Texas Instruments - XWR14xx mmWave Demo Visualization','NumberTitle','off')
else
    set(figHnd,'Name','Texas Instruments - Unknown platform mmWave Demo Visualization','NumberTitle','off')
end
warning off MATLAB:HandleGraphics:ObsoletedProperty:JavaFrame
jframe=get(figHnd,'javaframe');
jIcon=javax.swing.ImageIcon('texas_instruments.gif');
jframe.setFigureIcon(jIcon);
set(figHnd, 'MenuBar', 'none');
set(figHnd, 'Color', [0.8 0.8 0.8]);
set(figHnd, 'KeyPressFcn', @myKeyPressFcn)
set(figHnd,'ResizeFcn',@Resize_clbk);


pause(0.00001);
set(jframe,'Maximized',1); 
pause(0.00001);


% btn = uicontrol('Style', 'checkbox', 'String', '3D view',...
%         'Units','normalized',...
%         'Position', [.8 .95 .15 .03],...
%         'Value', 1,...
%         'Callback', @checkbox1_Callback);   

btn1 = uicontrol('Style', 'checkbox', 'String', 'Linear scale',...
        'Units','normalized',...
        'Position', [.05 .01  .15 .03],...
        'Value', 1,...
        'Callback', @checkbox_LinRangeProf_Callback);       
    
btn2 = uicontrol('Style', 'checkbox', 'String', 'Range/Doppler top view',...
        'Units','normalized',...
        'Position', [.55 .01  .15 .03],...
        'Value', 1,...
        'Callback', @checkbox_DopRngHeatMap_Callback);       

btn3 = uicontrol('Style', 'checkbox', 'String', 'record',...
        'Units','normalized',...
        'Position', [.55+.15 .01  .15 .03],...
        'Value', 0,...
        'Callback', @checkbox_Logging_Callback);       
    
warning off MATLAB:griddata:DuplicateDataPoints

    
%Read Configuration file
cliCfgFileId = fopen(cliCfgFileName, 'r');
if cliCfgFileId == -1
    fprintf('File %s not found!\n', cliCfgFileName);
    return
else
    fprintf('Opening configuration file %s ...\n', cliCfgFileName);
end
cliCfg=[];
tline = fgetl(cliCfgFileId);
k=1;
while ischar(tline)
    cliCfg{k} = tline;
    tline = fgetl(cliCfgFileId);
    k = k + 1;
end
fclose(cliCfgFileId);

global StatsInfo
StatsInfo.interFrameProcessingTime = 0;
StatsInfo.transmitOutputTime = 0;
StatsInfo.interFrameProcessingMargin = 0;
StatsInfo.interChirpProcessingMargin = 0;
StatsInfo.interFrameCPULoad = 0;
StatsInfo.activeFrameCPULoad = 0;


global activeFrameCPULoad
global interFrameCPULoad
activeFrameCPULoad = zeros(100,1);
interFrameCPULoad = zeros(100,1);
global guiCPULoad
guiCPULoad = zeros(100,1);


global guiProcTime
guiProcTime = 0;

displayUpdateCntr =0;



%Parse CLI parameters
Params = parseCfg(cliCfg);

log2ToLog10 = 20*log10(2);
if platformType == hex2dec('a1642')
    log2Qformat = 1/(256*Params.dataPath.numRxAnt*Params.dataPath.numTxAnt);
elseif platformType == hex2dec('a1443')
    if Params.dataPath.numTxAnt == 3
        log2Qformat = (4/3)*1/512;
    else
        log2Qformat = 1/512;
    end
else
    fprintf('Unknown platform\n');
    return
end

global maxRngProfYaxis
global maxRangeProfileYaxisLin
global maxRangeProfileYaxisLog
maxRangeProfileYaxisLin = maxRangeProfileYaxis;
maxRangeProfileYaxisLog = log2(maxRangeProfileYaxis) * log2ToLog10;
global rangeProfileLinearScale
rangeProfileLinearScale = 0;
if rangeProfileLinearScale == 1
    maxRngProfYaxis = maxRangeProfileYaxisLin;
else
    maxRngProfYaxis = maxRangeProfileYaxisLog;
end

global dopRngHeatMapFlag
dopRngHeatMapFlag = 1;

%Configure monitoring UART port 
sphandle = configureSport(comportSnum);

%Send Configuration Parameters to XWR16xx
%Open CLI port
if ischar(comportCliNum)
    comportCliNum=str2num(comportCliNum);
end
if loadCfg == 1
    spCliHandle = configureCliPort(comportCliNum);

    warning off; %MATLAB:serial:fread:unsuccessfulRead
    timeOut = get(spCliHandle,'Timeout');
    set(spCliHandle,'Timeout',1);
    tStart = tic;
    while 1
        fprintf(spCliHandle, ''); cc=fread(spCliHandle,100);
        cc = strrep(strrep(cc,char(10),''),char(13),'');
        if ~isempty(cc)
                break;
        end
        pause(0.1);
        toc(tStart);
    end
    set(spCliHandle,'Timeout', timeOut);
    warning on;
    
    
    %Send CLI configuration to XWR1xxx
    fprintf('Sending configuration to XWR1xxx %s ...\n', cliCfgFileName);
    for k=1:length(cliCfg)
        if isempty(strrep(strrep(cliCfg{k},char(9),''),char(32),''))
            continue;
        end
        if strcmp(cliCfg{k}(1),'%')
            continue;
        end
        fprintf(spCliHandle, cliCfg{k});
        fprintf('%s\n', cliCfg{k});
        for kk = 1:3
            cc = fgetl(spCliHandle);
            if strcmp(cc,'Done')
                fprintf('%s\n',cc);
                break;
            elseif ~isempty(strfind(cc, 'not recognized as a CLI command'))
                fprintf('%s\n',cc);
                return;
            elseif ~isempty(strfind(cc, 'Error'))
                fprintf('%s\n',cc);
                return;
            end
        end
%             pause(.5);
%         C = strsplit(cliCfg{k});
%         if strcmp(C{1},'sensorStop')
%             pause(5);
%         elseif strcmp(C{1},'flushCfg')
%             pause(5);
%         else
%             pause(.5);
%         end
        pause(0.2)
    end
    fclose(spCliHandle);
    delete(spCliHandle);
end
    
    
displayChirpParams(Params);
byteVecIdx = 0;
rangePlotYaxisMin = 1e10;
rangePlotXaxisMax = Params.dataPath.rangeIdxToMeters * Params.dataPath.numRangeBins;
tStart = tic;
tIdleStart = tic;
timeout_ctr = 0;
bytevec_cp_max_len = 2^15;
bytevec_cp = zeros(bytevec_cp_max_len,1);
bytevec_cp_len = 0;

packetNumberPrev = 0;

global loggingEnable
loggingEnable = 0;
global fidLog;
fidLog = 0;

%Initalize figures
figIdx = 2;
if (Params.guiMonitor.detectedObjects == 1)
    Params.guiMonitor.detectedObjectsFigHnd = subplot(Params.guiMonitor.numFigRow, Params.guiMonitor.numFigCol, figIdx);
    figIdx = figIdx + 1;
    hold off
    if Params.dataPath.numTxElevAnt == 1
        plot3(0,0,0,'ro','MarkerSize',5)
        hold on
        plot3([0 0],[0 3.5],[0 0],'-r')
        plot3([-2 2],[0 0],[0 0],'-r')
        plot3([0 0],[0 0],[-1 1],'-r')
        grid on          
        set(gca,'XLim',[-range_width range_width]);
        set(gca,'YLim',[0 range_depth]);      
        set(gca,'ZLim',[-1 1]);
        xlabel('meters');                  
        ylabel('meters');
        title('3D Scatter Plot')
        view(ELEV_VIEW);
    else
        hold on
        t = linspace(pi/6,5*pi/6,128);
        patch( [0 range_depth*cos(t) 0], [0 range_depth*sin(t) 0],  [0.0 0.0 0.5] );
        axis equal                    

        axis([-range_width range_width 0 range_depth])
        xlabel('Distance along lateral axis (meters)');                  
        ylabel('Distance along longitudinal axis (meters)');
        plotGrid(range_depth, range_grid_arc);
        title('X-Y Scatter Plot')
        set(gca,'Color',[0 0 0.5]);
        Params.guiMonitor.detectedObjectsPlotHnd = plot(0,0,'g.', 'Marker', '.','MarkerSize',5);
    end
end
            
if (Params.guiMonitor.logMagRange == 1)
    Params.guiMonitor.logMagRangeFigHnd = subplot(Params.guiMonitor.numFigRow, Params.guiMonitor.numFigCol, figIdx);
    Params.guiMonitor.logMagRangeUpdate = 0;
    Params.guiMonitor.logMagRangePlotHnd = plot(Params.guiMonitor.logMagRangeFigHnd, Params.dataPath.rangeIdxToMeters*[0:Params.dataPath.numRangeBins-1],zeros(length(Params.dataPath.rangeIdxToMeters*[0:Params.dataPath.numRangeBins-1]),3), '-');    
    hline = findobj(Params.guiMonitor.logMagRangePlotHnd, 'type', 'line');
    set(hline(1),'LineStyle','-', 'color',[0 0 1])
    set(hline(2),'LineStyle','none', 'color',[1 0 0], 'Marker', 'x')
    set(hline(3),'LineStyle','-', 'color',[0 .5 0])
    figIdx = figIdx + 1;
    hold on;
    a=zeros(4,1);
    a(4) = maxRngProfYaxis;
    a(2) = rangePlotXaxisMax; 
    axis(a);
    xlabel('Range (m)');
    title('Range Profile');
    grid on;
    
    np = NaN*zeros(Params.dataPath.numRangeBins,1);
end

if (Params.guiMonitor.detectedObjects == 1) && (Params.guiMonitor.rangeDopplerHeatMap ~=1)
    Params.guiMonitor.detectedObjectsRngDopFigHnd = subplot(Params.guiMonitor.numFigRow, Params.guiMonitor.numFigCol, figIdx);
    figIdx = figIdx + 1;
    hold off
    plot(0,0,'Color',[0 0 0.5])
    hold on
    set(gca,'Color',[0 0 0.5]);
    dopplerRange = Params.dataPath.dopplerResolutionMps * Params.dataPath.numDopplerBins / 2;
    axis([0 range_depth -dopplerRange dopplerRange])
    xlabel('Range (meters)');
    ylabel('Doppler (m/s)');
    title('Doppler-Range Plot')
    grid on;
    set(gca,'Xcolor',[0.5 0.5 0.5]);
    set(gca,'Ycolor',[0.5 0.5 0.5]);
    Params.guiMonitor.detectedObjectsRngDopPlotHnd = plot(0,0,'g.', 'Marker', '.','MarkerSize',5);
end            

if (Params.guiMonitor.rangeAzimuthHeatMap == 1)
    %Range complex bins at zero Doppler all virtual (azimuth) antennas
    Params.guiMonitor.rangeAzimuthHeatMapFigHnd = subplot(Params.guiMonitor.numFigRow, Params.guiMonitor.numFigCol, figIdx);
    figIdx = figIdx + 1;
    hold on;
    theta = asind([-NUM_ANGLE_BINS/2+1 : NUM_ANGLE_BINS/2-1]'*(2/NUM_ANGLE_BINS));
    range = [0:Params.dataPath.numRangeBins-1] * Params.dataPath.rangeIdxToMeters;
    xlin=linspace(-range_width,range_width,200);
    ylin=linspace(0,range_depth,200);
    view([0,90])
    set(gca,'Color',[0 0 0.5]);
    axis('equal')
    axis([-range_width range_width 0 range_depth])
    xlabel('Distance along lateral axis (meters)');                  
    ylabel('Distance along longitudinal axis (meters)');
    title('Azimuth-Range Heatmap')
end

if (Params.guiMonitor.rangeDopplerHeatMap == 1)
    %Get the whole log magnitude range dopppler matrix
    Params.guiMonitor.rangeDopplerHeatMapFigHnd = subplot(Params.guiMonitor.numFigRow, Params.guiMonitor.numFigCol, figIdx);
    Params.guiMonitor.rangeDopplerHeatMapPlotHnd = 0;
    figIdx = figIdx + 1;

    if dopRngHeatMapFlag == 1
        dopplerRange = Params.dataPath.dopplerResolutionMps * Params.dataPath.numDopplerBins / 2;
        axis([0 range_depth -dopplerRange dopplerRange]);
        xlabel('Range (meters)');
        ylabel('Doppler (m/s)');              
        title('Doppler-Range Heatmap')
    else
        a=zeros(4,1);
        a(4) = maxRngProfYaxis;
        a(3) = 0;
        a(2) = rangePlotXaxisMax; 
        axis(a);
        xlabel('Range (meters)');
        title('Range Profile')
    end
end
if (Params.guiMonitor.stats == 1)
     Params.guiMonitor.statsFigHnd = subplot(Params.guiMonitor.numFigRow, Params.guiMonitor.numFigCol, figIdx);
     Params.guiMonitor.statsPlotHnd = plot(zeros(100,3));
     figIdx = figIdx + 1;
     hold on;
     xlabel('frames');                  
     ylabel('% CPU Load');
     axis([0 100 0 100])
     title('Active and Interframe CPU Load')
     plot([0 0 0; 0 0 0])
     legend('Interframe', 'Active frame', 'GUI')
end

magicNotOkCntr=0;

%-------------------- Main Loop ------------------------
while (~EXIT_KEY_PRESSED)
    %Read bytes
    readUartCallbackFcn(sphandle, 0);
    
    if BYTES_AVAILABLE_FLAG == 1
        BYTES_AVAILABLE_FLAG = 0;
        %fprintf('bytevec_cp_len, bytevecAccLen = %d %d \n',bytevec_cp_len, bytevecAccLen)
        if (bytevec_cp_len + bytevecAccLen) < bytevec_cp_max_len
            bytevec_cp(bytevec_cp_len+1:bytevec_cp_len + bytevecAccLen) = bytevecAcc(1:bytevecAccLen);
            bytevec_cp_len = bytevec_cp_len + bytevecAccLen;
            bytevecAccLen = 0;
        else
            fprintf('Error: Buffer overflow, bytevec_cp_len, bytevecAccLen = %d %d \n',bytevec_cp_len, bytevecAccLen)
        end
    end
    
    bytevecStr = char(bytevec_cp);
    magicOk = 0;
    startIdx = strfind(bytevecStr', char([2 1 4 3 6 5 8 7]));
    if ~isempty(startIdx)
        if startIdx(1) > 1
            bytevec_cp(1: bytevec_cp_len-(startIdx(1)-1)) = bytevec_cp(startIdx(1):bytevec_cp_len);
            bytevec_cp_len = bytevec_cp_len - (startIdx(1)-1);
        end
        if bytevec_cp_len < 0
            fprintf('Error: %d %d \n',bytevec_cp_len, bytevecAccLen)
            bytevec_cp_len = 0;
        end

        totalPacketLen = sum(bytevec_cp(8+4+[1:4]) .* [1 256 65536 16777216]');                
        if bytevec_cp_len >= totalPacketLen
            magicOk = 1;
        else
            magicOk = 0;
        end
    end

    byteVecIdx = 0;
    if(magicOk == 1)
        %fprintf('OK, bytevec_cp_len = %d\n',bytevec_cp_len);
        if debugFlag
            fprintf('Frame Interval = %.3f sec,  ', toc(tStart));
        end
        tStart = tic;
        
        [Header, byteVecIdx] = getHeader(bytevec_cp, byteVecIdx);
        detObj.numObj = 0;
        for tlvIdx = 1:Header.numTLVs
            [tlv, byteVecIdx] = getTlv(bytevec_cp, byteVecIdx);
            switch tlv.type
                case MMWDEMO_UART_MSG_DETECTED_POINTS
                    if tlv.length >= OBJ_STRUCT_SIZE_BYTES
                        [detObj, byteVecIdx] = getDetObj(bytevec_cp, ...
                                byteVecIdx, ...
                                tlv.length, ...
                                Params.dataPath.rangeIdxToMeters, ...
                                Params.dataPath.dopplerResolutionMps, ...
                                Params.dataPath.numDopplerBins);
                    end
                case MMWDEMO_UART_MSG_RANGE_PROFILE
                    [rp, byteVecIdx] = getRangeProfile(bytevec_cp, ...
                                                byteVecIdx, ...
                                                tlv.length);
                case MMWDEMO_UART_MSG_NOISE_PROFILE
                    [np, byteVecIdx] = getRangeProfile(bytevec_cp, ...
                                                byteVecIdx, ...
                                                tlv.length);
                case MMWDEMO_UART_MSG_AZIMUT_STATIC_HEAT_MAP
                    [Q, byteVecIdx] = getAzimuthStaticHeatMap(bytevec_cp, ...
                                                byteVecIdx, ...
                                                Params.dataPath.numTxAzimAnt, ...
                                                Params.dataPath.numRxAnt,...
                                                Params.dataPath.numRangeBins,...
                                                NUM_ANGLE_BINS);


                case MMWDEMO_UART_MSG_RANGE_DOPPLER_HEAT_MAP
                    [rangeDoppler, byteVecIdx] = getRangeDopplerHeatMap(bytevec_cp, ...
                                                byteVecIdx, ...
                                                Params.dataPath.numDopplerBins, ...
                                                Params.dataPath.numRangeBins);
                case MMWDEMO_UART_MSG_STATS
                    [StatsInfo, byteVecIdx] = getStatsInfo(bytevec_cp, ...
                                                byteVecIdx);
                     %fprintf('StatsInfo: %d, %d, %d %d \n', StatsInfo.interFrameProcessingTime, StatsInfo.transmitOutputTime, StatsInfo.interFrameProcessingMargin, StatsInfo.interChirpProcessingMargin);
                     displayUpdateCntr = displayUpdateCntr + 1;
                     interFrameCPULoad = [interFrameCPULoad(2:end); StatsInfo.interFrameCPULoad];
                     activeFrameCPULoad = [activeFrameCPULoad(2:end); StatsInfo.activeFrameCPULoad];
                     guiCPULoad = [guiCPULoad(2:end); 100*guiProcTime/Params.frameCfg.framePeriodicity];
                     if displayUpdateCntr == 40
                        UpdateDisplayTable(Params);
                        displayUpdateCntr = 0;
                     end
                otherwise
            end
        end

        byteVecIdx = Header.totalPacketLen;

        if ((Header.frameNumber - packetNumberPrev) ~= 1) && (packetNumberPrev ~= 0)
            fprintf('Error: Packets lost: %d, current frame num = %d \n', (Header.frameNumber - packetNumberPrev - 1), Header.frameNumber)
        end
        packetNumberPrev = Header.frameNumber;

        %Log detected objects
        if (loggingEnable == 1)
            if (Header.numDetectedObj > 0)
                fprintf(fidLog, '%d %d\n', Header.frameNumber, detObj.numObj);
                fprintf(fidLog, '%.3f ', detObj.x);
                fprintf(fidLog, '\n');
                fprintf(fidLog, '%.3f ', detObj.y);
                fprintf(fidLog, '\n');
                if Params.dataPath.numTxElevAnt == 1
                    fprintf(fidLog, '%.3f ', detObj.z);
                    fprintf(fidLog, '\n');
                end
                fprintf(fidLog, '%.3f ', detObj.doppler);
                fprintf(fidLog, '\n');
            end
        end            

        if (Params.guiMonitor.detectedObjects == 1)
            if detObj.numObj > 0
                % Display Detected objects
                if Params.dataPath.numTxElevAnt == 1
                    %Plot detected objects in 3D
                    hold off
                    plot3(detObj.x,detObj.y,detObj.z,'x');hold on;
                    for kk=1:length(detObj.x)
                      plot3([detObj.x(kk) detObj.x(kk)],[detObj.y(kk) detObj.y(kk)],[0 detObj.z(kk)],'-');hold on;
                      plot3([0 detObj.x(kk)],[detObj.y(kk) detObj.y(kk)],[0 0],'-');hold on;
                    end

                    plot3(0,0,0,'ro','MarkerSize',5)
                    plot3([0 0],[0 3.5],[0 0],'-r')
                    plot3([-2 2],[0 0],[0 0],'-r')
                    plot3([0 0],[0 0],[-1 1],'-r')
                    view(ELEV_VIEW);
                else
                    %Plot detected objects in 2D
                    set(Params.guiMonitor.detectedObjectsPlotHnd, 'Xdata', detObj.x, 'Ydata', detObj.y);
                    title(Params.guiMonitor.detectedObjectsFigHnd, sprintf('X-Y Scatter Plot, %d objects', Header.numDetectedObj))
                end
            else
                if Params.dataPath.numTxElevAnt == 1
                    %Empty Plot detected objects in 3D
                    hold off
                    plot3(0,0,0,'ro','MarkerSize',5)
                    plot3([0 0],[0 3.5],[0 0],'-r')
                    plot3([-2 2],[0 0],[0 0],'-r')
                    plot3([0 0],[0 0],[-1 1],'-r')
                    view(ELEV_VIEW);
                else
                    %Empty plot detected objects in 2D
                    set(Params.guiMonitor.detectedObjectsPlotHnd, 'Xdata', [], 'Ydata', []);
                    title(Params.guiMonitor.detectedObjectsFigHnd, sprintf('X-Y Scatter Plot, %d objects', Header.numDetectedObj))
                end
            end
        end

        if (Params.guiMonitor.logMagRange == 1)
            if rangeProfileLinearScale == 1
                rp = Params.dspFftScaleCompAll_lin * 2.^(rp*log2Qformat);
                %rp = 2.^(rp*log2Qformat);
            else
                rp = rp*log2ToLog10*log2Qformat + Params.dspFftScaleCompAll_log;
            end
            
            %Plot range profile
            subplot(Params.guiMonitor.logMagRangeFigHnd);

            rpDet=NaN*zeros(size(rp));
            if (Params.guiMonitor.detectedObjects == 1)
                if detObj.numObj > 0
                    rpDet=NaN*zeros(size(rp));
                    zeroDopIdx = detObj.rangeIdx(detObj.doppler==0);
                    if ~isempty(zeroDopIdx)
                        if (min(zeroDopIdx) >= 0) && (max(zeroDopIdx) <= (length(rp)-1))
                            rpDet(detObj.rangeIdx(detObj.doppler==0)+1) = ...
                                rp(detObj.rangeIdx(detObj.doppler==0)+1); %matlab indexes from 1 (not zero)
                        else
                            fprintf('Error: Range index exceeds range dimension\n');
                        end
                    end
                end
            end
            if Params.guiMonitor.noiseProfile == 1
                if exist('np', 'var')
                    if rangeProfileLinearScale == 1
                        np = Params.dspFftScaleCompAll_lin * 2.^(np*log2Qformat);
                    else
                        np = np*log2ToLog10*log2Qformat + Params.dspFftScaleCompAll_log;
                    end
                end
            end
            if length(rp) ~= length(rpDet) || length(rp) ~= length(np)
                fprintf('Error: Lengths of rp, rpDet, np = %d,%d, %d \n', length(rp), length(rpDet), length(np));
            else
                set(Params.guiMonitor.logMagRangePlotHnd, {'Ydata'}, num2cell([rp rpDet np].',2));
            end
            
            if Params.guiMonitor.logMagRangeUpdate
                Params.guiMonitor.logMagRangeUpdate = 0;
                a=axis;
                a(4) = maxRngProfYaxis;
                a(2) = rangePlotXaxisMax; 
                axis(a);
            end
        end

        if (Params.guiMonitor.detectedObjects == 1) && (Params.guiMonitor.rangeDopplerHeatMap ~=1)
            if detObj.numObj > 0
                set(Params.guiMonitor.detectedObjectsRngDopPlotHnd, 'Xdata', detObj.range, 'Ydata', detObj.doppler);                
            else
                set(Params.guiMonitor.detectedObjectsRngDopPlotHnd, 'Xdata', [], 'Ydata', []);                
            end
        end            

        if Params.guiMonitor.rangeAzimuthHeatMap == 1
            %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
            %Range complex bins at zero Doppler all virtual (azimuth) antennas
            subplot(Params.guiMonitor.rangeAzimuthHeatMapFigHnd);
            hold off;

            theta = asind([-NUM_ANGLE_BINS/2+1 : NUM_ANGLE_BINS/2-1]'*(2/NUM_ANGLE_BINS));
            range = [0:Params.dataPath.numRangeBins-1] * Params.dataPath.rangeIdxToMeters;
            	if(save_flag==20)
            		log_idx=log_idx+1;
            		file_name_save=['Qa_' num2str(log_idx)];
            		file_name_save
			save(file_name_save, 'Q');
			save_flag=0;
			display('took a snap shot');
		else
			save_flag=save_flag+1;
		end
            posX = range' * sind(theta');
            posY = range' * cosd(theta');
            QQ=fftshift(abs(Q),1);
            QQ=QQ.';
            QQ=QQ(:,2:end);
            xlin=linspace(-range_width,range_width,200);
            ylin=linspace(0,range_depth,200);
            [X,Y]=meshgrid(xlin,ylin);
            warning off
            Z=griddata(posX,posY,fliplr(QQ),X,Y,'linear');
            warning on
            surf(xlin,ylin,Z);
            shading INTERP
            view([0,90])
            set(gca,'Color',[0 0 0.5]);
            axis('equal')
            axis([-range_width range_width 0 range_depth])
            xlabel('Distance along lateral axis (meters)');                  
            ylabel('Distance along longitudinal axis (meters)');
            title('Azimuth-Range Heatmap')
            	
        end

        if Params.guiMonitor.rangeDopplerHeatMap == 1
            %Get the whole log magnitude range dopppler matrix
            subplot(Params.guiMonitor.rangeDopplerHeatMapFigHnd);
            if (Params.guiMonitor.rangeDopplerHeatMapPlotHnd ~= 0)
                delete(Params.guiMonitor.rangeDopplerHeatMapPlotHnd);
                Params.guiMonitor.rangeDopplerHeatMapPlotHnd = 0;
            end
            if dopRngHeatMapFlag == 1
                Params.guiMonitor.rangeDopplerHeatMapPlotHnd = surf(Params.dataPath.rangeIdxToMeters*[0:Params.dataPath.numRangeBins-1],...
                    Params.dataPath.dopplerResolutionMps*[-Params.dataPath.numDopplerBins/2:Params.dataPath.numDopplerBins/2-1],...
                    rangeDoppler);
                view([0 90]);
                shading INTERP;
                dopplerRange = Params.dataPath.dopplerResolutionMps * (Params.dataPath.numDopplerBins/2-1);
                axis([0 range_depth -dopplerRange dopplerRange]);
                xlabel('Range (meters)');
                ylabel('Doppler (m/s)');              
                title('Doppler-Range Heatmap')
            else
                if rangeProfileLinearScale == 1
                    rangeDoppler = Params.dspFftScaleCompAll_lin * 2.^(rangeDoppler*log2Qformat);
                else
                    rangeDoppler = rangeDoppler*log2ToLog10*log2Qformat + Params.dspFftScaleCompAll_log;
                end

                Params.guiMonitor.rangeDopplerHeatMapPlotHnd = plot(Params.dataPath.rangeIdxToMeters*[0:Params.dataPath.numRangeBins-1],...
                     rangeDoppler');
                a=axis;
                a(4) = maxRngProfYaxis;
                a(3) = 0;
                a(2) = rangePlotXaxisMax; 
                axis(a);
                xlabel('Range (meters)');
                grid on
                title('Range Profile')
            end
        end
        if (Params.guiMonitor.stats == 1)
            set(Params.guiMonitor.statsPlotHnd, {'Ydata'}, num2cell([interFrameCPULoad activeFrameCPULoad guiCPULoad]',2));
        end
        guiProcTime = round(toc(tStart) * 1e3);
        if debugFlag
            fprintf('processing time %f secs \n',toc(tStart));
        end

    else
        magicNotOkCntr = magicNotOkCntr + 1;
        %fprintf('Magic word not found! cntr = %d\n', magicNotOkCntr);
    end

    %Remove processed data
    if byteVecIdx > 0
        shiftSize = byteVecIdx;
        bytevec_cp(1: bytevec_cp_len-shiftSize) = bytevec_cp(shiftSize+1:bytevec_cp_len);
        bytevec_cp_len = bytevec_cp_len - shiftSize;
        if bytevec_cp_len < 0
            fprintf('Error: bytevec_cp_len < bytevecAccLen, %d %d \n', bytevec_cp_len, bytevecAccLen)
            bytevec_cp_len = 0;
        end
    end
    if bytevec_cp_len > (bytevec_cp_max_len * 7/8)
        bytevec_cp_len = 0;
    end

    tIdleStart = tic;

    pause(0.001);
   
    
    if(toc(tIdleStart) > 2*Params.frameCfg.framePeriodicity/1000)
        timeout_ctr=timeout_ctr+1;
        if debugFlag == 1
            fprintf('Timeout counter = %d\n', timeout_ctr);
        end
        tIdleStart = tic;
    end
end
%close and delete handles before exiting
close(1); % close figure
fclose(sphandle); %close com port
delete(sphandle);
return

function [] = readUartCallbackFcn(obj, event)
global bytevecAcc;
global bytevecAccLen;
global readUartFcnCntr;
global BYTES_AVAILABLE_FLAG
global BYTES_AVAILABLE_FCN_CNT
global BYTE_VEC_ACC_MAX_SIZE

bytesToRead = get(obj,'BytesAvailable');
if(bytesToRead == 0)
    return;
end

[bytevec, byteCount] = fread(obj, bytesToRead, 'uint8');

if bytevecAccLen + length(bytevec) < BYTE_VEC_ACC_MAX_SIZE * 3/4
    bytevecAcc(bytevecAccLen+1:bytevecAccLen+byteCount) = bytevec;
    bytevecAccLen = bytevecAccLen + byteCount;
else
    bytevecAccLen = 0;
end

readUartFcnCntr = readUartFcnCntr + 1;
BYTES_AVAILABLE_FLAG = 1;
return

function [] = dispError()
disp('error!');
return

function [sphandle] = configureSport(comportSnum)
    global BYTES_AVAILABLE_FCN_CNT;

    if ~isempty(instrfind('Type','serial'))
        disp('Serial port(s) already open. Re-initializing...');
        delete(instrfind('Type','serial'));  % delete open serial ports.
    end
    comportnum_str=['COM' num2str(comportSnum)]
    sphandle = serial(comportnum_str,'BaudRate',921600);
    set(sphandle,'InputBufferSize', 2^16);
    set(sphandle,'Timeout',10);
    set(sphandle,'ErrorFcn',@dispError);
    set(sphandle,'BytesAvailableFcnMode','byte');
    set(sphandle,'BytesAvailableFcnCount', 2^16+1);%BYTES_AVAILABLE_FCN_CNT);
    set(sphandle,'BytesAvailableFcn',@readUartCallbackFcn);
    fopen(sphandle);
return

function [sphandle] = configureCliPort(comportPnum)
    %if ~isempty(instrfind('Type','serial'))
    %    disp('Serial port(s) already open. Re-initializing...');
    %    delete(instrfind('Type','serial'));  % delete open serial ports.
    %end
    comportnum_str=['COM' num2str(comportPnum)]
    sphandle = serial(comportnum_str,'BaudRate',115200);
    set(sphandle,'Parity','none')    
    set(sphandle,'Terminator','LF')    

    
    fopen(sphandle);
return

function []=ComputeCR_SNR(noiseEnergy,rp,Params)

cr_range=floor([1.0 5.0]/Params.dataPath.rangeIdxToMeters);

[maxVal  max_idx]=max(rp(cr_range(1): cr_range(2)));

noiseSigma=sqrt(noiseEnergy/(256*2*8));

noiseSigma_dB=10*log10(abs(noiseSigma));

maxValdB=10*maxVal/(log2(10)*2^9);

SNRdB=maxValdB-noiseSigma_dB;
dist= (max_idx+cr_range(1)-1)*Params.dataPath.rangeIdxToMeters;
%[SNRdB dist]
%keyboard
return


% function checkbox1_Callback(hObject, eventdata, handles)
% global ELEV_VIEW
% % hObject    handle to checkbox1 (see GCBO)
% % eventdata  reserved - to be defined in a future version of MATLAB
% % handles    structure with handles and user data (see GUIDATA)
% 
% % Hint: get(hObject,'Value') returns toggle state of checkbox1
% 
% if (get(hObject,'Value') == get(hObject,'Max'))
%   ELEV_VIEW = 3;
% else
%   ELEV_VIEW = 2;
% end

function checkbox_LinRangeProf_Callback(hObject, eventdata, handles)
global rangeProfileLinearScale
global Params
if (get(hObject,'Value') == get(hObject,'Max'))
  rangeProfileLinearScale = 1;
else
  rangeProfileLinearScale = 0;
end
global maxRngProfYaxis
global maxRangeProfileYaxisLin
global maxRangeProfileYaxisLog
if rangeProfileLinearScale == 1
    maxRngProfYaxis = maxRangeProfileYaxisLin;
else
    maxRngProfYaxis = maxRangeProfileYaxisLog;
end
Params.guiMonitor.logMagRangeUpdate = 1;
return

function checkbox_DopRngHeatMap_Callback(hObject, eventdata, handles)
global dopRngHeatMapFlag

if (get(hObject,'Value') == get(hObject,'Max'))
    dopRngHeatMapFlag = 1;
else
    dopRngHeatMapFlag = 0;
end
return

function checkbox_Logging_Callback(hObject, eventdata, handles)
global loggingEnable
global fidLog;

if (get(hObject,'Value') == get(hObject,'Max'))
    loggingEnable = 1;
    fid = fopen('lognum.dat', 'r');
    if fid ~= -1
        logNum = fscanf(fid, '%d');
        fclose(fid);
    else
        logNum = 0;
    end
    logNum = logNum +1;
    fid = fopen('lognum.dat', 'w');
    fprintf(fid, '%d\n', logNum);
    fclose(fid);
    fidLog = fopen(sprintf('log_%03d.dat',logNum),'w');
else
    loggingEnable = 0;
    if fidLog ~= 0
        fclose(fidLog);
    end

end
return
%Display Chirp parameters in table on screen
function displayChirpParams(Params)
global hndChirpParamTable
global StatsInfo
global guiProcTime
    if hndChirpParamTable ~= 0
        delete(hndChirpParamTable);
        hndChirpParamTable = 0;
    end

    dat =  {'Start Frequency (Ghz)', Params.profileCfg.startFreq;...
            'Slope (MHz/us)', Params.profileCfg.freqSlopeConst;...   
            'Samples per chirp', Params.profileCfg.numAdcSamples;...
            'Chirps per frame',  Params.dataPath.numChirpsPerFrame;...
            'Sampling rate (Msps)', Params.profileCfg.digOutSampleRate / 1000;...
            'Bandwidth (GHz)', Params.profileCfg.freqSlopeConst * Params.profileCfg.numAdcSamples /...
                               Params.profileCfg.digOutSampleRate;...
            'Range resolution (m)', Params.dataPath.rangeResolutionMeters;...
            'Velocity resolution (m/s)', Params.dataPath.dopplerResolutionMps;...
            'Number of Tx (MIMO)', Params.dataPath.numTxAnt;...
            'Frame periodicity (msec)', Params.frameCfg.framePeriodicity;...
            'InterFrameProcessingTime (usec)', StatsInfo.interFrameProcessingTime; ...
            'transmitOutputTime (usec)', StatsInfo.transmitOutputTime; ...
            'interFrameProcessingMargin (usec)', StatsInfo.interFrameProcessingMargin; ...
            'InterChirpProcessingMargin (usec)', StatsInfo.interChirpProcessingMargin; ...
            'GuiProcTime (msec)', guiProcTime;            
            };
        
    columnname =   {'___________Parameter (Units)___________', 'Value'};
    columnformat = {'char', 'numeric'};
    
    t = uitable('Units','normalized','Position',...
                [0.05 0.55 0.8/Params.guiMonitor.numFigCol 0.4], 'Data', dat,... 
                'ColumnName', columnname,...
                'ColumnFormat', columnformat,...
                'ColumnWidth', 'auto',...
                'RowName',[]); 
            
    %Center table in lower left quarter            
    tPos = get(t, 'Extent');
    if(tPos(4) > 0.9*1/2)
        tPos(4) = 0.9*1/2;
    end
    if(tPos(3) > 0.9*1/Params.guiMonitor.numFigCol)
        tPos(3) = 0.9*1/Params.guiMonitor.numFigCol;
    end
    tPos(1) = 1/Params.guiMonitor.numFigCol / 2 - tPos(3)/2;
    tPos(2) = 1/2 + 1/2 / 2 - tPos(4)/2;
    set(t, 'Position',tPos)

    hndChirpParamTable = t;
return

function UpdateDisplayTable(Params)
global hndChirpParamTable
global StatsInfo
global guiProcTime
global Header
    t = hndChirpParamTable;
    dat = get(t, 'Data');
    row=11;
    dat{row, 2} = StatsInfo.interFrameProcessingTime;
    dat{row+1, 2} = StatsInfo.transmitOutputTime;
    dat{row+2, 2} = StatsInfo.interFrameProcessingMargin;
    dat{row+3, 2} = StatsInfo.interChirpProcessingMargin;
    dat{row+4, 2} = guiProcTime;
    set(t,'Data', dat);
return
%Read relevant CLI parameters and store into P structure
function [P] = parseCfg(cliCfg)
global TOTAL_PAYLOAD_SIZE_BYTES
global MAX_NUM_OBJECTS
global OBJ_STRUCT_SIZE_BYTES
global platformType
global STATS_SIZE_BYTES

    P=[];
    for k=1:length(cliCfg)
        C = strsplit(cliCfg{k});
        if strcmp(C{1},'channelCfg')
            P.channelCfg.txChannelEn = str2num(C{3});
            if platformType == hex2dec('a1642')
                P.dataPath.numTxAzimAnt = bitand(bitshift(P.channelCfg.txChannelEn,0),1) +...
                                          bitand(bitshift(P.channelCfg.txChannelEn,-1),1);
                P.dataPath.numTxElevAnt = 0;
            elseif platformType == hex2dec('a1443')
                P.dataPath.numTxAzimAnt = bitand(bitshift(P.channelCfg.txChannelEn,0),1) +...
                                          bitand(bitshift(P.channelCfg.txChannelEn,-2),1);
                P.dataPath.numTxElevAnt = bitand(bitshift(P.channelCfg.txChannelEn,-1),1);
            else
                fprintf('Unknown platform \n');
                return
            end
            P.channelCfg.rxChannelEn = str2num(C{2});
            P.dataPath.numRxAnt = bitand(bitshift(P.channelCfg.rxChannelEn,0),1) +...
                                  bitand(bitshift(P.channelCfg.rxChannelEn,-1),1) +...
                                  bitand(bitshift(P.channelCfg.rxChannelEn,-2),1) +...
                                  bitand(bitshift(P.channelCfg.rxChannelEn,-3),1);
            P.dataPath.numTxAnt = P.dataPath.numTxElevAnt + P.dataPath.numTxAzimAnt;
                                
        elseif strcmp(C{1},'dataFmt')
        elseif strcmp(C{1},'profileCfg')
            P.profileCfg.startFreq = str2num(C{3});
            P.profileCfg.idleTime =  str2num(C{4});
            P.profileCfg.rampEndTime = str2num(C{6});
            P.profileCfg.freqSlopeConst = str2num(C{9});
            P.profileCfg.numAdcSamples = str2num(C{11});
            P.profileCfg.digOutSampleRate = str2num(C{12}); %uints: ksps
        elseif strcmp(C{1},'chirpCfg')
        elseif strcmp(C{1},'frameCfg')
            P.frameCfg.chirpStartIdx = str2num(C{2});
            P.frameCfg.chirpEndIdx = str2num(C{3});
            P.frameCfg.numLoops = str2num(C{4});
            P.frameCfg.numFrames = str2num(C{5});
            P.frameCfg.framePeriodicity = str2num(C{6});
        elseif strcmp(C{1},'guiMonitor')
            P.guiMonitor.detectedObjects = str2num(C{2});
            P.guiMonitor.logMagRange = str2num(C{3});
            P.guiMonitor.noiseProfile = str2num(C{4});
            P.guiMonitor.rangeAzimuthHeatMap = str2num(C{5});
            P.guiMonitor.rangeDopplerHeatMap = str2num(C{6});
            P.guiMonitor.stats = str2num(C{7});
        end
    end
    P.dataPath.numChirpsPerFrame = (P.frameCfg.chirpEndIdx -...
                                            P.frameCfg.chirpStartIdx + 1) *...
                                            P.frameCfg.numLoops;
    P.dataPath.numDopplerBins = P.dataPath.numChirpsPerFrame / P.dataPath.numTxAnt;
    P.dataPath.numRangeBins = pow2roundup(P.profileCfg.numAdcSamples);
    P.dataPath.rangeResolutionMeters = 3e8 * P.profileCfg.digOutSampleRate * 1e3 /...
                     (2 * P.profileCfg.freqSlopeConst * 1e12 * P.profileCfg.numAdcSamples);
    P.dataPath.rangeIdxToMeters = 3e8 * P.profileCfg.digOutSampleRate * 1e3 /...
                     (2 * P.profileCfg.freqSlopeConst * 1e12 * P.dataPath.numRangeBins);
    P.dataPath.dopplerResolutionMps = 3e8 / (2*P.profileCfg.startFreq*1e9 *...
                                        (P.profileCfg.idleTime + P.profileCfg.rampEndTime) *...
                                        1e-6 * P.dataPath.numDopplerBins * P.dataPath.numTxAnt);
    %Calculate monitoring packet size
    tlSize = 8 %TL size 8 bytes
    TOTAL_PAYLOAD_SIZE_BYTES = 32; % size of header
    P.guiMonitor.numFigures = 1; %One figure for numerical parameers
    if P.guiMonitor.detectedObjects == 1 && P.guiMonitor.rangeDopplerHeatMap == 1
        TOTAL_PAYLOAD_SIZE_BYTES = TOTAL_PAYLOAD_SIZE_BYTES +...
            OBJ_STRUCT_SIZE_BYTES*MAX_NUM_OBJECTS + tlSize;
        P.guiMonitor.numFigures = P.guiMonitor.numFigures + 1; %1 plots: X/Y plot
    end
    if P.guiMonitor.detectedObjects == 1 && P.guiMonitor.rangeDopplerHeatMap ~= 1
        TOTAL_PAYLOAD_SIZE_BYTES = TOTAL_PAYLOAD_SIZE_BYTES +...
            OBJ_STRUCT_SIZE_BYTES*MAX_NUM_OBJECTS + tlSize;
        P.guiMonitor.numFigures = P.guiMonitor.numFigures + 2; %2 plots: X/Y plot and Y/Doppler plot
    end
    if P.guiMonitor.logMagRange == 1
        TOTAL_PAYLOAD_SIZE_BYTES = TOTAL_PAYLOAD_SIZE_BYTES +...
            P.dataPath.numRangeBins * 2 + tlSize;
        P.guiMonitor.numFigures = P.guiMonitor.numFigures + 1;
    end
    if P.guiMonitor.noiseProfile == 1 && P.guiMonitor.logMagRange == 1
        TOTAL_PAYLOAD_SIZE_BYTES = TOTAL_PAYLOAD_SIZE_BYTES +...
            P.dataPath.numRangeBins * 2 + tlSize;
    end
    if P.guiMonitor.rangeAzimuthHeatMap == 1
        TOTAL_PAYLOAD_SIZE_BYTES = TOTAL_PAYLOAD_SIZE_BYTES +...
            (P.dataPath.numTxAzimAnt * P.dataPath.numRxAnt) * P.dataPath.numRangeBins * 4 + tlSize;
        P.guiMonitor.numFigures = P.guiMonitor.numFigures + 1; 
    end
    if P.guiMonitor.rangeDopplerHeatMap == 1
        TOTAL_PAYLOAD_SIZE_BYTES = TOTAL_PAYLOAD_SIZE_BYTES +...
            P.dataPath.numDopplerBins * P.dataPath.numRangeBins * 2 + tlSize;
        P.guiMonitor.numFigures = P.guiMonitor.numFigures + 1;
    end
    if P.guiMonitor.stats == 1
        TOTAL_PAYLOAD_SIZE_BYTES = TOTAL_PAYLOAD_SIZE_BYTES +...
            STATS_SIZE_BYTES + tlSize;
        P.guiMonitor.numFigures = P.guiMonitor.numFigures + 1;
    end
    TOTAL_PAYLOAD_SIZE_BYTES = 32 * floor((TOTAL_PAYLOAD_SIZE_BYTES+31)/32);
    P.guiMonitor.numFigRow = 2;
    P.guiMonitor.numFigCol = ceil(P.guiMonitor.numFigures/P.guiMonitor.numFigRow);
    if platformType == hex2dec('a1642')
        [P.dspFftScaleComp2D_lin, P.dspFftScaleComp2D_log] = dspFftScalComp2(16, P.dataPath.numDopplerBins);
        [P.dspFftScaleComp1D_lin, P.dspFftScaleComp1D_log]  = dspFftScalComp1(64, P.dataPath.numRangeBins);
    else
        [P.dspFftScaleComp1D_lin, P.dspFftScaleComp1D_log] = dspFftScalComp2(32, P.dataPath.numRangeBins);
        P.dspFftScaleComp2D_lin = 1;
        P.dspFftScaleComp2D_log = 0;
    end
    P.dspFftScaleCompAll_lin = P.dspFftScaleComp2D_lin * P.dspFftScaleComp1D_lin;
    P.dspFftScaleCompAll_log = P.dspFftScaleComp2D_log + P.dspFftScaleComp1D_log;    
    
return

function [y] = pow2roundup (x)
    y = 1;
    while x > y
        y = y * 2;
    end
return

function myKeyPressFcn(hObject, event)
    global EXIT_KEY_PRESSED
    if lower(event.Key) == 'q'
        EXIT_KEY_PRESSED  = 1;
    end

return

function Resize_clbk(hObject, event)
global Params
displayChirpParams(Params);
return

function []=plotGrid(R,range_grid_arc)

sect_width=pi/12;  
offset_angle=pi/6:sect_width:5*pi/6;
r=[0:range_grid_arc:R];
w=linspace(pi/6,5*pi/6,128);

for n=2:length(r)
    plot(real(r(n)*exp(1j*w)),imag(r(n)*exp(1j*w)),'color',[0.5 0.5 0.5], 'linestyle', ':')
end


for n=1:length(offset_angle)
    plot(real([0 R]*exp(1j*offset_angle(n))),imag([0 R]*exp(1j*offset_angle(n))),'color',[0.5 0.5 0.5], 'linestyle', ':')
end
return

function [Header, idx] = getHeader(bytevec, idx)
    idx = idx + 8; %Skip magic word
    word = [1 256 65536 16777216]';
    Header.version = sum(bytevec(idx+[1:4]) .* word);
    idx = idx + 4;
    Header.totalPacketLen = sum(bytevec(idx+[1:4]) .* word);
    idx = idx + 4;
    Header.platform = sum(bytevec(idx+[1:4]) .* word);
    idx = idx + 4;
    Header.frameNumber = sum(bytevec(idx+[1:4]) .* word);
    idx = idx + 4;
    Header.timeCpuCycles = sum(bytevec(idx+[1:4]) .* word);
    idx = idx + 4;
    Header.numDetectedObj = sum(bytevec(idx+[1:4]) .* word);
    idx = idx + 4;
    Header.numTLVs = sum(bytevec(idx+[1:4]) .* word);
    idx = idx + 4;
return

function [tlv, idx] = getTlv(bytevec, idx)
    word = [1 256 65536 16777216]';
    tlv.type = sum(bytevec(idx+(1:4)) .* word);
    idx = idx + 4;
    tlv.length = sum(bytevec(idx+(1:4)) .* word);
    idx = idx + 4;
return

function [detObj, idx] = getDetObj(bytevec, idx, tlvLen, rangeIdxToMeters, dopplerResolutionMps, numDopplerBins)
    global OBJ_STRUCT_SIZE_BYTES;
    detObj =[];
    detObj.numObj = 0;
    if tlvLen > 0
        %Get detected object descriptor
        word = [1 256]';
        detObj.numObj = sum(bytevec(idx+(1:2)) .* word);
        idx = idx + 2;
        xyzQFormat = 2^sum(bytevec(idx+(1:2)) .* word);
        idx = idx + 2;
        
        %Get detected array of detected objects
        bytes = bytevec(idx+(1:detObj.numObj*OBJ_STRUCT_SIZE_BYTES));
        idx = idx + detObj.numObj*OBJ_STRUCT_SIZE_BYTES;

        bytes = reshape(bytes, OBJ_STRUCT_SIZE_BYTES, detObj.numObj);
        detObj.rangeIdx = (bytes(1,:)+bytes(2,:)*256);
        detObj.range = detObj.rangeIdx * rangeIdxToMeters; %convert range index to range (in meters)
        detObj.dopplerIdx = (bytes(3,:)+bytes(4,:)*256); %convert doppler index to doppler (meters/sec)
        %circshift the doppler fft bins
        detObj.dopplerIdx(detObj.dopplerIdx > numDopplerBins/2-1) = ...
                detObj.dopplerIdx(detObj.dopplerIdx > numDopplerBins/2-1) - numDopplerBins;
        detObj.doppler = detObj.dopplerIdx * dopplerResolutionMps; %convert doppler index to doppler (meters/sec)
        detObj.peakVal = bytes(5,:)+bytes(6,:)*256; %peak value (16-bit=> so constructed from 2 bytes)

        detObj.x = bytes(7,:)+bytes(8,:)*256;
        detObj.y = bytes(9,:)+bytes(10,:)*256;
        detObj.z = bytes(11,:)+bytes(12,:)*256;
        detObj.x( detObj.x > 32767) =  detObj.x( detObj.x > 32767) - 65536;
        detObj.y( detObj.y > 32767) =  detObj.y( detObj.y > 32767) - 65536;
        detObj.z( detObj.z > 32767) =  detObj.z( detObj.z > 32767) - 65536;
        detObj.x =  detObj.x / xyzQFormat;
        detObj.y =  detObj.y / xyzQFormat;
        detObj.z =  detObj.z / xyzQFormat;

    end
return

function [rp, idx] = getRangeProfile(bytevec, idx, len)
    rp = bytevec(idx+(1:len));
    idx = idx + len;
    rp=rp(1:2:end)+rp(2:2:end)*256;
return

function [Q, idx] = getAzimuthStaticHeatMap(bytevec, idx, numTxAzimAnt, numRxAnt, numRangeBins, numAngleBins)
    len = numTxAzimAnt * numRxAnt * numRangeBins * 4;
    q = bytevec(idx+(1:len));
    idx = idx + len;
    q = q(1:2:end)+q(2:2:end)*256;
    q(q>32767) = q(q>32767) - 65536;
    q = q(1:2:end)+1j*q(2:2:end);
    q = reshape(q, numTxAzimAnt * numRxAnt, numRangeBins);
    Q = fft(q, numAngleBins);
return

function [rangeDoppler, idx] = getRangeDopplerHeatMap(bytevec, idx, numDopplerBins, numRangeBins)
    len = numDopplerBins * numRangeBins * 2;
    rangeDoppler = bytevec(idx+(1:len));
    idx = idx + len;
    rangeDoppler = rangeDoppler(1:2:end) + rangeDoppler(2:2:end)*256;
    rangeDoppler = reshape(rangeDoppler, numDopplerBins, numRangeBins);
    rangeDoppler = fftshift(rangeDoppler,1);
return

function [StatsInfo, idx] = getStatsInfo(bytevec, idx)
    word = [1 256 65536 16777216]';
    StatsInfo.interFrameProcessingTime = sum(bytevec(idx+(1:4)) .* word);
    idx = idx + 4;
    StatsInfo.transmitOutputTime = sum(bytevec(idx+(1:4)) .* word);
    idx = idx + 4;
    StatsInfo.interFrameProcessingMargin = sum(bytevec(idx+(1:4)) .* word);
    idx = idx + 4;
    StatsInfo.interChirpProcessingMargin = sum(bytevec(idx+(1:4)) .* word);
    idx = idx + 4;
    StatsInfo.activeFrameCPULoad = sum(bytevec(idx+(1:4)) .* word);
    idx = idx + 4;
    StatsInfo.interFrameCPULoad = sum(bytevec(idx+(1:4)) .* word);
    idx = idx + 4;
return

function [sLin, sLog] = dspFftScalComp2(fftMinSize, fftSize)
    sLin = fftMinSize/fftSize;
    sLog = 20*log10(sLin);
return

function [sLin, sLog] = dspFftScalComp1(fftMinSize, fftSize)
    smin =  (2.^(ceil(log2(fftMinSize)./log2(4)-1)))  ./ (fftMinSize);
    sLin =  (2.^(ceil(log2(fftSize)./log2(4)-1)))  ./ (fftSize);
    sLin = sLin / smin;
    sLog = 20*log10(sLin);
return
