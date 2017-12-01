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
from MQTTPubSub import MQTTPubSub


mqttQueue = Queue()


plt.ion()
plt.figure()

range_width = 5  # set to area you want to inspect
range_depth = 5  # set to area you want to inspect



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

xlin = np.linspace(-range_width, range_width, 100)
ylin = np.linspace(0, range_depth, 100)

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




def onMessage(mqttc, obj, msg):



    mqttQueue.put(np.fromstring(msg.payload,dtype=np.complex_))


params = {}
params["url"] = "10.156.14.6"
params["port"] = 2333
params["timeout"] = 10
params["topic"] = "radar"
params["onMessage"] = onMessage
params["username"] = "rbccps"
params["password"] = "rbccps@123"

server = MQTTPubSub(params)
server_rc = server.run()



	


while (True):

    while mqttQueue.empty() != True:
        z = np.reshape(mqttQueue.get(), (numTxAzimAnt * numRxAnt, numRangeBins), order='F')
        Z = fft(z, 64, axis=0)
        QQ = fftshift(np.absolute(Z), 0)
        Qq = np.transpose(QQ)
        qq = np.delete(Qq, 0, axis=1)

        gd = griddata(inPts, fliplr(qq).ravel(), outPts, 'nearest')
        plt.contourf(X, Y, gd, antialiasing=True, extent=extent)
        plt.pause(0.01)
