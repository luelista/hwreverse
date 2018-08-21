from abc import abstractmethod
import struct, binascii, datetime
class CommandTypes:
    #Command Types
    #Nicht verschlüsselt:
    FRAGMENT_ACK = 0
    ANSWER_WITHOUT_SECURITY = 1
    CONNECTION_REQUEST = 2
    CONNECTION_INFO = 3
    PAIRING_REQUEST = 4
    STATUS_CHANGED_NOTIFICATION = 5
    CLOSE_CONNECTION = 6
    BOOTLOADER_START_APP = 16
    BOOTLOADER_DATA = 17
    BOOTLOADER_STATUS = 18
    #Verschlüsselt  = 128-191
    ANSWER_WITH_SECURITY = 129
    STATUS_REQUEST = 130
    STATUS_INFO = 131
    MOUNT_OPTIONS_REQUEST = 132
    MOUNT_OPTIONS_INFO = 133
    MOUNT_OPTIONS_SET = 134
    COMMAND = 135
    AUTO_RELOCK_SET = 136
    PAIRING_SET = 138
    USER_LIST_REQUEST = 139
    USER_LIST_INFO = 140
    USER_REMOVE = 141
    USER_INFO_REQUEST = 142
    USER_INFO = 143
    USER_NAME_SET = 144
    USER_OPTIONS_SET = 145
    USER_PROG_REQUEST = 146
    USER_PROG_INFO = 147
    USER_PROG_SET = 148
    AUTO_RELOCK_PROG_REQUEST = 149
    AUTO_RELOCK_PROG_INFO = 150
    AUTO_RELOCK_PROG_SET = 151
    LOG_REQUEST = 152
    LOG_INFO = 153
    KEY_BLE_APPLICATION_BOOTLOADER_CALL = 154
    DAYLIGHT_SAVING_TIME_OPTIONS_REQUEST = 155
    DAYLIGHT_SAVING_TIME_OPTIONS_INFO = 156
    DAYLIGHT_SAVING_TIME_OPTIONS_SET = 157
    FACTORY_RESET = 158

def is_encrypted_command_type(commandType):
    return (commandType & 0xC0) == 0x80

def parse_command(data, cryptoMgr):
    commandType = data[0]
    if is_encrypted_command_type(commandType):
        encr = SecureCommand.parse(data)
        data = cryptoMgr.decrypt_packet(encr)
        commandType = data[0]
    
    if commandType == CommandTypes.CONNECTION_INFO:
        return ConnectionInfoCommand.parse(data)
    elif commandType == CommandTypes.PAIRING_SET:
        return PairingSetCommand.parse(data)
    elif commandType == CommandTypes.FRAGMENT_ACK:
        return FragmentAckCommand.parse(data)
    elif commandType == CommandTypes.ANSWER_WITHOUT_SECURITY:
        return AnswerWithoutSecurityCommand.parse(data)
    elif commandType == CommandTypes.ANSWER_WITH_SECURITY:
        return AnswerWithSecurityCommand.parse(data)
    elif commandType == CommandTypes.STATUS_INFO:
        return StatusInfoCommand.parse(data)
    else:
        return RawCommand(commandType, data[1:])


class AbstractCommand:
    def __init__(self, cmdType, cmdData):
        self.type = cmdType
        self.data = cmdData

    @abstractmethod
    def getCommandType(self):
        pass

    @abstractmethod
    def getCommandData(self):
        pass

    def serialize(self):
        return struct.pack('!B', self.getCommandType()) + self.getCommandData()

class RawCommand(AbstractCommand):
    def __init__(self, cmdType, cmdData):
        self.type = cmdType
        self.data = cmdData

    def getCommandType(self):
        return self.type

    def getCommandData(self):
        return self.data
    def __str__(self):
        return "UnknownCommand(type=%d, data=%s)" % (
                self.type, binascii.hexlify(self.data)
        )
    ACTION_CODE_LOCK = 0
    ACTION_CODE_UNLOCK = 1
    ACTION_CODE_OPEN = 2
RawCommand.COMMAND_LOCK = RawCommand(CommandTypes.COMMAND, bytes([RawCommand.ACTION_CODE_LOCK]))
RawCommand.COMMAND_UNLOCK = RawCommand(CommandTypes.COMMAND, bytes([RawCommand.ACTION_CODE_UNLOCK]))
RawCommand.COMMAND_OPEN = RawCommand(CommandTypes.COMMAND, bytes([RawCommand.ACTION_CODE_OPEN]))

class AnswerCommand(AbstractCommand):
    def __init__(self, flags):
        self.flags = flags
    def __str__(self):
        return "AnswerCommand(flags=0x%02X)" % (
                self.flags
        )
    def isFirstUser(self):
        return (self.flags & 0x81) == 1
    def isSuccess(self):
        return (self.flags & 0x80) == 0

class AnswerWithSecurityCommand(AnswerCommand):
    def parse(data):
        if data[0] != CommandTypes.ANSWER_WITH_SECURITY:
            raise "Invalid command type"
        return AnswerWithoutSecurityCommand(flags = data[1])
    def getCommandType(self):
        return CommandTypes.ANSWER_WITH_SECURITY

class AnswerWithoutSecurityCommand(AnswerCommand):
    def parse(data):
        if data[0] != CommandTypes.ANSWER_WITHOUT_SECURITY:
            raise "Invalid command type"
        return AnswerWithoutSecurityCommand(flags = data[1])
    def getCommandType(self):
        return CommandTypes.ANSWER_WITHOUT_SECURITY

class ConnectionRequestCommand(AbstractCommand):
    """
    applicationNonce: 8 bytes from secure random
    """
    def __init__(self, userNumber, applicationNonce):
        self.userNumber = userNumber
        self.applicationNonce = applicationNonce
    def __str__(self):
        return "ConnectionRequestCommand(userNumber=%d, applicationNonce=%s)" % (
                self.userNumber, binascii.hexlify(self.applicationNonce)
        )
    def getCommandType(self):
        return CommandTypes.CONNECTION_REQUEST
    def getCommandData(self):
        return struct.pack("!B8s", self.userNumber, self.applicationNonce)

class ConnectionInfoCommand(AbstractCommand):
    def __init__(self, pairingCtr, deviceNonce, reserved, bootloaderVersion, applicationVersion):
        self.pairingCtr = pairingCtr
        self.deviceNonce = deviceNonce
        self.reserved = reserved
        self.bootloaderVersion = bootloaderVersion
        self.applicationVersion = applicationVersion
    def __str__(self):
        return "ConnectionInfoCommand(pairingCtr=%d, deviceNonce=%s, reserved=%d, bootloaderVersion=%d, applicationVersion=%d)" % (
                self.pairingCtr, binascii.hexlify(self.deviceNonce), self.reserved, self.bootloaderVersion, self.applicationVersion
        )
    def parse(data):
        if data[0] != CommandTypes.CONNECTION_INFO:
            raise "Invalid command type"
        return ConnectionInfoCommand(
            pairingCtr = data[1],
            deviceNonce = data[2:10],
            reserved = data[10],
            bootloaderVersion = data[11],
            applicationVersion = data[12])
    def getCommandType(self):
        return CommandTypes.CONNECTION_INFO


class StatusRequestCommand(AbstractCommand):
    def __init__(self, dateTime=None):
        self.dateTime = dateTime if not dateTime is None else datetime.datetime.now()
    def __str__(self):
        return "StatusRequestCommand(dateTime=%s)" % (self.dateTime)
    def getCommandType(self):
        return CommandTypes.STATUS_REQUEST
    def getCommandData(self):
        return struct.pack("!BBBBBB", 
                self.dateTime.year - 2000, self.dateTime.month, self.dateTime.day, self.dateTime.hour, self.dateTime.minute, self.dateTime.second)

class StatusInfoCommand(AbstractCommand):
    def __init__(self, controlFlags, statusFlags, lockFlags, reserved, bootloaderVersion, applicationVersion):
        self.controlFlags = controlFlags
        self.statusFlags = statusFlags
        self.lockFlags = lockFlags
        self.reserved = reserved
        self.bootloaderVersion = bootloaderVersion
        self.applicationVersion = applicationVersion
    def __str__(self):
        return ("StatusInfoCommand(controlFlags=0x%02X (isDeviceControllable=%s, userRightType=%d), statusFlags=0x%02X (isBatteryLow=%s, isPairingEnabled=%s, isSystemTimeInvalid=%s), "+
        "lockFlags=0x%02X (getLockStatus=%d, self.isUnlockReady=%s, self.isAutoRelockActive=%s, self.isAutoRelockByProgActive=%s), reserved=%d, bootloaderVersion=%d, applicationVersion=%d)") % (
                self.controlFlags, self.isDeviceControllable(), self.getUserRightType(),
                 self.statusFlags, self.isBatteryLow(), self.isPairingEnabled(), self.isSystemTimeInvalid(),
                 self.lockFlags, self.getLockStatus(), self.isUnlockReady(), self.isAutoRelockActive(), self.isAutoRelockByProgActive(),
                 self.reserved, self.bootloaderVersion, self.applicationVersion
        )
    def parse(data):
        if data[0] != CommandTypes.STATUS_INFO:
            raise "Invalid command type"
        return StatusInfoCommand(
            controlFlags = data[1],
            statusFlags = data[2],
            lockFlags = data[3],
            reserved = data[4],
            bootloaderVersion = data[5],
            applicationVersion = data[6])
    def getCommandType(self):
        return CommandTypes.STATUS_INFO
    def getLockStatus(self):
        return self.lockFlags & 0x07
    def isAutoRelockActive(self):
        return (self.lockFlags & 16) != 0
    def isAutoRelockByProgActive(self):
        return (self.lockFlags & 32) != 0
    def isUnlockReady(self):
        return (self.lockFlags & 8) != 0

    def isBatteryLow(self):
        return (self.statusFlags & 128) != 0
    def isPairingEnabled(self):
        return (self.statusFlags & 1) != 0
    def isSystemTimeInvalid(self):
        return (self.statusFlags & 16) != 0

    def isDeviceControllable(self):
        return (self.controlFlags & 64) != 0
    def getUserRightType(self):
        return (self.controlFlags & 48) >> 4

class SecureCommand(AbstractCommand):
    def __init__(self, commandType, encryptedData, securityCounter, authValue):
        self.commandType = commandType
        self.encryptedData = encryptedData
        self.securityCounter = securityCounter
        self.authValue = authValue
    def parse(data):
        securityCounter, authValue = struct.unpack("!H4s", data[len(data)-6:])
        return SecureCommand(
            commandType=data[0],
            encryptedData = data[1:len(data)-6],
            securityCounter=securityCounter,
            authValue=authValue)
    def __str__(self):
        return "SecureCommand(commandType=%d, encryptedData=%s, securityCounter=%d, authValue=%s)" % (
                self.commandType, binascii.hexlify(self.encryptedData), 
                self.securityCounter, binascii.hexlify(self.authValue)
        )
    def getCommandType(self):
        return self.commandType
    def getCommandData(self):
        return self.encryptedData + struct.pack("!H4s", self.securityCounter, self.authValue)


class PairingRequestCommand(AbstractCommand):
    def __init__(self, pairingCtr, encryptedData, securityCounter, authValue):
        self.pairingCtr = pairingCtr
        self.encryptedData = encryptedData
        self.securityCounter = securityCounter
        self.authValue = authValue
    def __str__(self):
        return "PairingRequestCommand(pairingCtr=%d, encryptedData=%s, securityCounter=%d, authValue=%s)" % (
                self.pairingCtr, binascii.hexlify(self.encryptedData), 
                self.securityCounter, binascii.hexlify(self.authValue)
        )
    def getCommandType(self):
        return CommandTypes.PAIRING_REQUEST
    def getCommandData(self):
        return struct.pack("!B22sH4s", 
                self.pairingCtr, self.encryptedData, self.securityCounter, self.authValue)


class PairingSetCommand(AbstractCommand):
    def __init__(self, success):
        self.success = success
    def __str__(self):
        return "PairingSetCommand(success=%d)" % (
                self.success
        )
    
    def parse(data):
        if data[0] != CommandTypes.PAIRING_SET:
            raise "Invalid command type"
        return PairingSetCommand(True if data[1] == 1 else False)

class FragmentAckCommand(AbstractCommand):
    def __init__(self, fragmentHeader):
        self.fragmentHeader = fragmentHeader
    def parse(data):
        if data[0] != CommandTypes.FRAGMENT_ACK:
            raise "Invalid command type"
        return FragmentAckCommand(data[1])
    def getCommandType(self):
        return CommandTypes.FRAGMENT_ACK
    def getCommandData(self):
        return struct.pack("!B", self.fragmentHeader)
    def getFragmentRemaining():
        return self.fragmentHeader() & 127


class UserNameSetCommand(AbstractCommand):
    def __init__(self, userNumber, userName):
        self.userNumber = userNumber
        self.userName = userName
    def getCommandType(self):
        return CommandTypes.USER_NAME_SET
    def getCommandData(self):
        return struct.pack("!B20s", self.userNumber, self.userName.encode("ascii"))

