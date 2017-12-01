import numpy as np
import serial
from numpy.fft import fft
from numpy.fft import fftshift
from scipy.interpolate import griddata
import matplotlib.pyplot as plt
from numpy import fliplr
import time
from multiprocessing import Queue
import threading
import sys
import signal
import zmq
from multiprocessing import Process

def server():
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    socket.setsockopt_string(zmq.SUBSCRIBE, '')
    socket.bind("tcp://*:%s" % 5555)
    while True:
        message = socket.recv()
        print("Received request  %s" % message)



Process(target=server).start()

ser = serial.Serial('/dev/ttyACM1', 921600)


def signal_handler(signal, frame):
    ser.close()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)


serialQueue = Queue()

header = bytearray(b'\x02\x01\x04\x03\x06\x05\x08\x07')
word = [pow(256, i) for i in range(4)]
bytevec = []
bytes_array = []
q = []
qq = []
buff = []
# plt.ion()
plt.figure()

range_width = 3  # set to area you want to inspect
range_depth = 3  # set to area you want to inspect
# f=open('newReceive','w')
rangeAzimuthHeatMap = 0


def pow2roundup(x):
    y = 1
    while (x > y):
        y = y * 2
    return y


numAngleBins = 64  # corresponding to the mmw code
numTxAzimAnt = 2  # numTxAzimAnt=((txChannelEn >> 0) & 1) + ((txChannelEn >> 1) & 1)
numRxAnt = 4  # numRxAnt=((rxChannelEn >> 0) & 1) + ((rxChannelEn >> 1) & 1) + ((rxChannelEn >> 2) & 1) + ((rxChannelEn >> 3) & 1)
numRangeBins = pow2roundup(256)
rangeIdxToMeters = 0.047392004  # 3*5500/(20*68*numRangeBins)

theta = np.reshape(np.arcsin(np.arange(-31, 32) / 32.0), (1, 63))
rangeMap = np.arange(256) * rangeIdxToMeters
posX = np.matmul(np.reshape(rangeMap, (256, 1)), np.sin(theta))
posY = np.matmul(np.reshape(rangeMap, (256, 1)), np.cos(theta))

xlin = np.linspace(-range_width, range_width, 50)
ylin = np.linspace(0, range_depth, 50)

X, Y = np.meshgrid(xlin, ylin)
rangeAzBinLength = numTxAzimAnt * numRxAnt * numRangeBins * 4
q2 = [0] * int(rangeAzBinLength / 2)
extent = [xlin[0], xlin[-1], ylin[0], ylin[-1]]
q3 = np.zeros(int(rangeAzBinLength / 4), dtype=np.complex_)
frameIndices = {}
k = 0
i = 0
numTerms = int(rangeAzBinLength / 2)
numCplxTerms = int(numTerms / 2)
inPts = (posX.ravel(), posY.ravel())
outPts = (X, Y)
readBytes = 0

def readSerial():
    global serialQueue
    while True:

        serialQueue.put(ser.read(65536))


serialThread = threading.Thread(target=readSerial)
serialThread.start()



while (True):

    try:


        while serialQueue.empty() != True:

            buff = serialQueue.get()
            frameIndices[0] = buff.find(header)  # Find header in buffer
            if frameIndices[0] != -1:
                while frameIndices[k] != -1:
                    frameIndices[k + 1] = buff.find(header, frameIndices[k] + 8)
                    k = k + 1
                frameIndices.pop(k)  # Populate all header indices, except the last (not found -1)
                k = 0

                bytesAvailable = 0
                for frame in range(0, len(frameIndices) - 1):


                    line = bytearray(buff[frameIndices[frame]:frameIndices[frame + 1]])
                    idx = 28  # After ignoring header, version, totalPacketLength, frameNumber, timeCpuCycles, platform

                    numDetectedObj = np.dot(line[idx:idx + 4], word)
                    idx = idx + 4

                    numTLVs = np.dot(line[idx:idx + 4], word)
                    idx = idx + 4

                    for tlvIdx in range(0, numTLVs):
                        TLVtype = 0
                        TLVlength = 0
                        TLVtype = np.dot(line[idx:idx + 4], word)
                        idx += 4
                        TLVlength = np.dot(line[idx:idx + 4], word)
                        idx += 4

                        # To refactor later
                        if TLVtype == 1:
                            if TLVlength >= 12:
                                numObj = line[idx] + line[idx + 1] * 256
                                idx += 2
                                exponent = line[idx] + line[idx + 1] * 256
                                xyzQFormat = pow(2, exponent)
                                idx += 2
                                for i in range(0, numObj * 12):
                                    bytes_array.append(line[idx + i])  # Temporary placeholder
                                idx += numObj * 12
                                detObjArray = np.reshape(bytes_array, (12, numObj))
                                rangeIdx = detObjArray[0, :] + detObjArray[1, :] * 256
                                bytes_array = []
                        # End


                        if TLVtype == 2:
                            idx = idx + TLVlength
                        if TLVtype == 3:
                            idx = idx + TLVlength

                        if TLVtype == 4:

                            for i in range(0, rangeAzBinLength):
                                q.append(line[idx + i])

                            idx = idx + rangeAzBinLength
                            q1 = bytearray(q)
                            c = 0
                            for i in range(0, numTerms):
                                q2[i] = q1[c] + q1[c + 1] * 256
                                c += 2
                            for i in range(0, numTerms):
                                if q2[i] > 32767:
                                    q2[i] = q2[i] - 65536
                            c = 0
                            for i in range(0, numCplxTerms):
                                q3[i] = q2[c] + 1j * q2[c + 1]
                                c = c + 2

                            z = np.reshape(q3, (numTxAzimAnt * numRxAnt, numRangeBins), order='F')
                            Z = fft(z, 64, axis=0)

                            q = []
                            bytevec = []

                            QQ = fftshift(np.absolute(Z), 0)
                            Qq = np.transpose(QQ)
                            qq = np.delete(Qq, 0, axis=1)

                            gd = griddata(inPts, fliplr(qq).ravel(), outPts, 'nearest')

                            ## Simple surface plot example
                            ## x, y values are not specified, so assumed to be 0:50

                            time_now = time.time()
                            plt.contourf(X, Y, gd)
                            plt.pause(0.01)
                            time.sleep(0.1)
                            idx = 0

                            i = 0



    except Exception as e:
        print(e)

