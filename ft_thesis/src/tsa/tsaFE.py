__author__ = 'orenstal'
import socket
import json
import sys
import logging
import time


def printLogMsg(level, functionName, msg):
    msgToLog = '[' + functionName + '] ' + msg

    if level == logging.CRITICAL:
        logging.critical(msgToLog)
    elif level == logging.ERROR:
        logging.error(msgToLog)
    elif level == logging.WARNING:
        logging.warning(msgToLog)
    elif level == logging.INFO:
        logging.info(msgToLog)
    elif level == logging.DEBUG:
        logging.debug(msgToLog)



class tsaFE(object):

    # parameters between the tsa FE and cleanup manager server
    NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX = 7
    NUM_OF_DIGITS_FOR_COMMAND_PREFIX = 1

    # commands between the tsa FE and cleanup manager server
    REPLAY_PACKETS_TSA_COMMAND_TYPE = 4
    CLEAR_PACKETS_TSA_COMMAND_TYPE = 5
    REPLAY_AND_CLEAR_PACKETS_TSA_COMMAND_TYPE = 6
    STOP_MASTER_TSA_COMMAND_TYPE = 7

    # commands between the tsa FE and the tsa BE
    ADD_POLICY_CHAIN_CLI = "addpolicychain"
    REMOVE_POLICY_CHAIN_CLI = "removepolicychain"
    # UNREGISTER_MIDDLEBOX_CLI = "unregmb"
    # REPLACE_MASTER_MB_CLI = "replacemaster"

    # commands between the tsa FE and cleanup manager server
    REPLAY_CLI = "replay"
    CLEAR_CLI = "clear"
    REPLAY_AND_CLEAR_CLI = "r&c"
    REPLAY_AND_CLEAR_DEBUG_CLI = "r&cd"   # debug mode - run this command 10 times
    STOP_MASTER_MIDDLEBOX_CLI = "stopmaster"

    EXIT_CLI = "exit"

    # parameters for json communication
    COMMAND_STRING = "command"
    ARGUMENTS_COMMAND = "arguments"
    DATA_STRING = "data"
    RETURN_VALUE_STRING = "return value"
    SUCCESS_STRING = "success"

    # parameters for the tsaBE server
    TSA_BE_LISTENING_IP = '127.0.0.1'  #'10.0.0.101'
    TSA_BE_LISTENING_PORT = 9093

    # parameters for the cleanup manager server
    CLEANUP_MANAGER_LISTENING_IP = '10.0.0.100'
    CLEANUP_MANAGER_LISTENING_PORT = 9001

    def __init__(self):
        self.sockToCleanupManager = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sockToCleanupManager.connect((tsaFE.CLEANUP_MANAGER_LISTENING_IP, tsaFE.CLEANUP_MANAGER_LISTENING_PORT))

        self.sockToTsaBE = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sockToTsaBE.connect((tsaFE.TSA_BE_LISTENING_IP, tsaFE.TSA_BE_LISTENING_PORT))

        self.startCommandLine()



    def closeSockets(self):
        self.sockToCleanupManager.close()
        self.sockToTsaBE.close()



    def sendToCleanupManagerServer(self, command, arguments):
        argumentsLen = self.getArgumentsLenAsString(arguments)
        preparedCommand = self.getCommandLenAsString(command)
        data = argumentsLen + preparedCommand + arguments
        print "send the following data: %s\n" %data
        self.sockToCleanupManager.send(data)
        res = self.sockToCleanupManager.recv(1024)
        print "received: res: %s" %str(res)

        return res



    def getArgumentsLenAsString(self, arguments):
        print "arguemnts is: %s" %(str(arguments))
        argumentsLen = str(len(arguments)).zfill(tsaFE.NUM_OF_DIGITS_FOR_MSG_LEN_PREFIX)
        print "argumentsLen: %s" %argumentsLen

        return argumentsLen



    def getCommandLenAsString(self, command):
        command = str(command)
        print "command is: %s" %(command)

        commandLen = command.zfill(tsaFE.NUM_OF_DIGITS_FOR_COMMAND_PREFIX)
        print "commandLenAsString: %s" %(commandLen)

        return commandLen



    def sendToTsaBE(self, data):
        self.sockToTsaBE.send(json.dumps(data) + '\n')
        res = self.sockToTsaBE.recv(1024)
        result = json.loads(res)

        return result



    def toJSONRequestFormat(self, command, arguments):
        request = {}
        request[tsaFE.COMMAND_STRING] = command
        request[tsaFE.ARGUMENTS_COMMAND] = arguments

        return request



    def getSlaveMbOnStop(self, stoppedMbId):
        oldMbId = int(stoppedMbId)
        if oldMbId == 6:
            return "7"
        elif oldMbId == 7:
            return "10"

        return str(oldMbId)




    def startCommandLine(self):
        menuMsg =  "\nPlease enter one of the following options:\n"
        menuMsg += "addPolicyChain <policyChain, including the first host> {dl_dst=, dl_src=, nw_src=, nw_dst=, tp_src=, tp_dst=}\n"
        menuMsg += "removePolicyChain <policyChain, including the first host> {dl_dst=, dl_src=, nw_src=, nw_dst=, tp_src=, tp_dst=}\n"
        menuMsg += "replay <mb id>\n"
        menuMsg += "clear <mb id>\n"
        menuMsg += "r&c <mb id>\n"
        menuMsg += "r&cd <mb id>\n"
        menuMsg += "stopmaster <old mb id>\n"
        menuMsg += "exit\n"
        menuMsg += "------------------------------\n"

        while True:
            userInput = raw_input(menuMsg).lower()

            if userInput is None or userInput == "":
                continue

            splitted = userInput.split()
            command = splitted[0]
            splitted.remove(command)
            arguments = ' '.join(splitted)

            if command == tsaFE.ADD_POLICY_CHAIN_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'add policychain' command")
                request = self.toJSONRequestFormat(command, arguments)
                result = self.sendToTsaBE(request)

                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result[tsaFE.DATA_STRING]))

            elif command == tsaFE.REMOVE_POLICY_CHAIN_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'remove policychain' command")
                request = self.toJSONRequestFormat(command, arguments)
                result = self.sendToTsaBE(request)

                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result[tsaFE.DATA_STRING]))

            elif command == tsaFE.REPLAY_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'replay' command")

                print "replay from master mb: %s" %str(arguments)
                result = self.sendToCleanupManagerServer(tsaFE.REPLAY_PACKETS_TSA_COMMAND_TYPE, arguments)

                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result))

            elif command == tsaFE.CLEAR_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'clear' command")

                print "clear master mb: %s and its slave" %str(arguments)
                result = self.sendToCleanupManagerServer(tsaFE.CLEAR_PACKETS_TSA_COMMAND_TYPE, arguments)

                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result))

            elif command == tsaFE.REPLAY_AND_CLEAR_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'replay and clear' command")

                print "replay and clear master mb: %s" %str(arguments)
                result = self.sendToCleanupManagerServer(tsaFE.REPLAY_AND_CLEAR_PACKETS_TSA_COMMAND_TYPE, arguments)

                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result))

            elif command == tsaFE.REPLAY_AND_CLEAR_DEBUG_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'replay and clear - debug mode. run 10 times automatically' command")

                for i in range(10):
                    print "[%d] replay and clear master mb: %s" %(i, str(arguments))
                    result = self.sendToCleanupManagerServer(tsaFE.REPLAY_AND_CLEAR_PACKETS_TSA_COMMAND_TYPE, arguments)
                    printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result))
                    time.sleep(1)    # 1 sec

                print "replay and clear master mb %s is done" %(str(arguments))

            elif command == tsaFE.STOP_MASTER_MIDDLEBOX_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'stop master middlebox' command")

                print "stopped mb is: %s" %str(arguments)
                oldMasterMbName = 'm'+arguments
                newMb = self.getSlaveMbOnStop(arguments)
                arguments += "," + str(newMb)
                result = self.sendToCleanupManagerServer(tsaFE.STOP_MASTER_TSA_COMMAND_TYPE, arguments)

                printLogMsg(logging.INFO, "startCommandLine", "replay and clear all packets from failed mb. result: %s" %str(result))

                request = self.toJSONRequestFormat(command, oldMasterMbName)
                result = self.sendToTsaBE(request)

                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result[tsaFE.DATA_STRING]))

            elif command == tsaFE.EXIT_CLI:
                printLogMsg(logging.INFO, "startCommandLine", "processing 'exit' command")
                request = self.toJSONRequestFormat(command, {})
                result = self.sendToTsaBE(request)

                self.closeSockets()
                printLogMsg(logging.INFO, "startCommandLine", "%s" %str(result[tsaFE.RETURN_VALUE_STRING]))
                break

            else:
                printLogMsg(logging.WARNING, "startCommandLine", "Illegal command")


debugMode = False
if len(sys.argv) == 2 and sys.argv[1] == "debug":
    debugMode = True

if debugMode:
    logging.basicConfig(level=logging.DEBUG, format='%(asctime)s - %(levelname)s - %(message)s')
else:
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

tsaFE()