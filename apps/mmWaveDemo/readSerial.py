import cBind


ser = cBind.serial()

while True:

    ser.read()
