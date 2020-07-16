#include "VescUart.h"
#include <HardwareSerial.h>

VescUart::VescUart(void){
	nunchuck.valueX         = 127;
	nunchuck.valueY         = 127;
	nunchuck.lowerButton  	= false;
	nunchuck.upperButton  	= false;
}

void VescUart::setSerialPort(HardwareSerial* port)
{
	serialPort = port;
}

void VescUart::setDebugPort(Stream* port)
{
	debugPort = port;
}



/**
 * @brief
 * Eunchan added. this take message from buffer
 *
 * @param  payloadReceived : payload will be stored here.
 * @param  receivedByte : everytime, from loop. Serial.read() will be passed here
 * @return int : length of payload
 */

uint16_t counter = 0;
uint16_t endMessage = 256;
bool messageRead = false;
uint8_t messageReceived[256] = {255};
uint16_t lenPayload = 0;
int VescUart::receiveBufferMessage(uint8_t * payloadReceived, const uint8_t & receivedByte) {

	// static uint16_t counter = 0;
	// static uint16_t endMessage = 256;
	// static bool messageRead = false;
	// static uint8_t messageReceived[256] = {255};
	// static uint16_t lenPayload = 0;

    // step 1. collect messageReceived[]
    // step 2.do the same thing

    //step 1. collect
    // counter 1 : 2(flag)
    // counter 2 : length
    // counter 3 : some registered codes
    if (counter < 3){
        messageReceived[counter++] = receivedByte;
        if (messageReceived[0] != 2){
            counter = 0;
            endMessage = 256;
            messageRead = false;
            return 0;
        }

        if (counter == 3) {
            if (messageReceived[2] != 4 && messageReceived[2] != 47 && messageReceived[1] != 73 && messageReceived[1] != 62 ){
                counter = 0;
                endMessage = 256;
                messageRead = false;
                return 0;
            }
            switch (messageReceived[0])
            {
                case 2:
                    endMessage = messageReceived[1] + 5; //Payload size + 2 for sice + 3 for SRC and End.
                    lenPayload = messageReceived[1];
                break;

                case 3:
                    // ToDo: Add Message Handling > 255 (starting with 3)
                    if( debugPort != NULL ){
                        debugPort->println("Message is larger than 256 bytes - not supported");
                    }
                    counter = 0;
                    endMessage = 256;
                    messageRead = false;
                break;

                default:
                    if( debugPort != NULL ){
                        debugPort->println("Unvalid start bit");
                    }
                      // initialize static variable as default
                    counter = 0;
                    endMessage = 256;
                    messageRead = false;
                break;
            }
        }
        return 0;

    } else if (counter < messageReceived[1] + 4){
        messageReceived[counter++] = receivedByte;
        return 0;
    }
    else {
        // step 2.  process
        messageReceived[counter] = receivedByte;

        if (messageReceived[endMessage - 1] == 3){
            messageReceived[endMessage] = 0;
            if (debugPort != NULL) {
                debugPort->println("End of message reached!");
            }
            messageRead = true;
        }

    }


	if(messageRead == false) {
        if (debugPort != NULL ){
		    debugPort->println("Timeout");
        }
            // initialize static variable as default
        counter = 0;
        endMessage = 256;
        messageRead = false;
        return 0;
	}

	bool unpacked = false;

	if (messageRead) {
		unpacked = unpackPayload(messageReceived, endMessage, payloadReceived);
	}


    // initialize static variable as default
    counter = 0;
	endMessage = 256;
	messageRead = false;


	if (unpacked) {
		// Message was read
		return lenPayload;
	}
	else {
		// No Message Read
		return 0;
	}


}


int VescUart::receiveUartMessage(uint8_t * payloadReceived) {

	// Messages <= 255 starts with "2", 2nd byte is length
	// Messages > 255 starts with "3" 2nd and 3rd byte is length combined with 1st >>8 and then &0xFF

	uint16_t counter = 0;
	uint16_t endMessage = 256;
	bool messageRead = false;
	uint8_t messageReceived[256];
	uint16_t lenPayload = 0;

	uint32_t timeout = millis() + 100; // Defining the timestamp for timeout (100ms before timeout)

	while ( millis() < timeout && messageRead == false) {

		while (serialPort->available()) {

			messageReceived[counter++] = serialPort->read();

			if (counter == 2) {

				switch (messageReceived[0])
				{
					case 2:
						endMessage = messageReceived[1] + 5; //Payload size + 2 for sice + 3 for SRC and End.
						lenPayload = messageReceived[1];
					break;

					case 3:
						// ToDo: Add Message Handling > 255 (starting with 3)
						if( debugPort != NULL ){
							debugPort->println("Message is larger than 256 bytes - not supported");
						}
					break;

					default:
						if( debugPort != NULL ){
							debugPort->println("Unvalid start bit");
						}
					break;
				}
			}

			if (counter >= sizeof(messageReceived)) {
				break;
			}

			if (counter == endMessage && messageReceived[endMessage - 1] == 3) {
				messageReceived[endMessage] = 0;
				if (debugPort != NULL) {
					debugPort->println("End of message reached!");
				}
				messageRead = true;
				break; // Exit if end of message is reached, even if there is still more data in the buffer.
			}
		}
	}
	if(messageRead == false && debugPort != NULL ) {
		debugPort->println("Timeout");
	}

	bool unpacked = false;

	if (messageRead) {
		unpacked = unpackPayload(messageReceived, endMessage, payloadReceived);
	}

	if (unpacked) {
		// Message was read
		return lenPayload;
	}
	else {
		// No Message Read
		return 0;
	}
}


bool VescUart::unpackPayload(uint8_t * message, int lenMes, uint8_t * payload) {

	uint16_t crcMessage = 0;
	uint16_t crcPayload = 0;

	// Rebuild crc:
	crcMessage = message[lenMes - 3] << 8;
	crcMessage &= 0xFF00;
	crcMessage += message[lenMes - 2];

	if(debugPort!=NULL){
		debugPort->print("SRC received: "); debugPort->println(crcMessage);
	}

	// Extract payload:
	memcpy(payload, &message[2], message[1]);
    crcPayload = crc16(payload, message[1]);

	if( debugPort != NULL ){
		debugPort->print("SRC calc: "); debugPort->println(crcPayload);
	}

	if (crcPayload == crcMessage) {
		if( debugPort != NULL ) {
			debugPort->print("Received: ");
			serialPrint(message, lenMes); debugPort->println();

			debugPort->print("Payload :      ");
			serialPrint(payload, message[1] - 1); debugPort->println();
		}

		return true;
	}else{
		return false;
	}

}


int VescUart::packSendPayload(uint8_t * payload, int lenPay) {

	uint16_t crcPayload = crc16(payload, lenPay);
	int count = 0;
	uint8_t messageSend[256];

	if (lenPay <= 256)
	{
		messageSend[count++] = 2;
		messageSend[count++] = lenPay;
	}
	else
	{
		messageSend[count++] = 3;
		messageSend[count++] = (uint8_t)(lenPay >> 8);
		messageSend[count++] = (uint8_t)(lenPay & 0xFF);
	}

	memcpy(&messageSend[count], payload, lenPay);

	count += lenPay;
	messageSend[count++] = (uint8_t)(crcPayload >> 8);
	messageSend[count++] = (uint8_t)(crcPayload & 0xFF);
	messageSend[count++] = 3;
	messageSend[count] = '\0';

	if(debugPort!=NULL){
		debugPort->print("UART package send: "); serialPrint(messageSend, count);
	}

	// Sending package
	serialPort->write(messageSend, count);

	// Returns number of send bytes
	return count;
}


inline int VescUart::processReadPacket(uint8_t * message) {

	COMM_PACKET_ID packetId;
	int32_t ind = 0;

	packetId = (COMM_PACKET_ID)message[0];
	message++; // Removes the packetId from the actual message (payload)

	switch (packetId){
        //4
		case COMM_GET_VALUES: // Structure defined here: https://github.com/vedderb/bldc/blob/43c3bbaf91f5052a35b75c2ff17b5fe99fad94d1/commands.c#L164

			data.tempFetFiltered 	= buffer_get_float16(message, 10.0, &ind);
            data.tempMotorFiltered 	= buffer_get_float16(message, 10.0, &ind);

			data.avgMotorCurrent 	= buffer_get_float32(message, 100.0, &ind);
			data.avgInputCurrent 	= buffer_get_float32(message, 100.0, &ind);


			ind += 8; // Skip the next 8 bytes
            //mc_interface_read_reset_avg_id
            //mc_interface_read_reset_avg_iq

			data.dutyCycleNow 		= buffer_get_float16(message, 1000.0, &ind);
			data.rpm 				= buffer_get_int32(message, &ind);
			data.inpVoltage 		= buffer_get_float16(message, 10.0, &ind);
			data.ampHours 			= buffer_get_float32(message, 10000.0, &ind);
			data.ampHoursCharged 	= buffer_get_float32(message, 10000.0, &ind);
			ind += 8; // Skip the next 8 bytes
			data.tachometer 		= buffer_get_int32(message, &ind);
			data.tachometerAbs 		= buffer_get_int32(message, &ind);
            return packetId;
        break;


    //47
        case COMM_GET_VALUES_SETUP: // Structure defined here: https://github.com/vedderb/bldc/blob/master/commands.c

			data.tempFetFiltered 	= buffer_get_float16(message, 10.0, &ind);
            data.tempMotorFiltered 	= buffer_get_float16(message, 10.0, &ind);

            // data.currentTot 	    = buffer_get_float32(message, 100.0, &ind);
            // data.currentInTot 	    = buffer_get_float32(message, 100.0, &ind);
	        data.avgMotorCurrent 	= buffer_get_float32(message, 100.0, &ind);
			data.avgInputCurrent 	= buffer_get_float32(message, 100.0, &ind);

            data.dutyCycleNow 	    = buffer_get_float16(message, 1000.0, &ind);
            data.rpm 	            = buffer_get_int32(message, &ind);
            data.speed 	            = buffer_get_float32(message, 1000.0, &ind);
            data.inpVoltage 		= buffer_get_float16(message, 10.0, &ind);
            data.batteryLevel 		= buffer_get_float16(message, 1000.0, &ind);

            data.ampHours 		    = buffer_get_float32(message, 10000.0, &ind);
			data.ampHoursCharged 	= buffer_get_float32(message, 10000.0, &ind);

            ind += 8;

            data.tachometer 		= buffer_get_int32(message, &ind);
			data.tachometerAbs 		= buffer_get_int32(message, &ind);
            return packetId;
		break;

        case COMM_GET_DECODED_PPM:
            data.ppmLevel 			= buffer_get_int32(message, &ind);
            data.lastPulseLen 		= buffer_get_int32(message, &ind);
            return packetId;
        break;

		default:
			return packetId;
		break;
	}
}

int VescUart::getVescValues(const uint8_t & received_byte) {

	static uint8_t payload[80];

	int lenPayload = receiveBufferMessage(payload, received_byte);

	if (lenPayload > 10) {
		int read = processReadPacket(payload); //returns true if sucessful
		return read;
	}
	else
	{
		return 0;
	}
}



void VescUart::requestPPMValues(void) {

	uint8_t command[1] = { COMM_GET_DECODED_PPM };
	uint8_t payload[256];
	packSendPayload(command, 1);
}


void VescUart::requestVescGetValues(void) {

	uint8_t command[1] = { COMM_GET_VALUES };
	uint8_t payload[256];
	packSendPayload(command, 1);
}

bool VescUart::getVescValues(void) {

	uint8_t command[1] = { COMM_GET_VALUES };
	uint8_t payload[256];

	packSendPayload(command, 1);
	// delay(1); //needed, otherwise data is not read

	int lenPayload = receiveUartMessage(payload);

	if (lenPayload > 55) {
		bool read = processReadPacket(payload); //returns true if sucessful
		return read;
	}
	else
	{
		return false;
	}
}

void VescUart::serialPrint(uint8_t * data, int len) {
	if(debugPort != NULL){
		for (int i = 0; i <= len; i++)
		{
			debugPort->print(data[i]);
			debugPort->print(" ");
		}

		debugPort->println("");
	}
}

void VescUart::printVescValues() {
	if(debugPort != NULL){
		debugPort->print("avgMotorCurrent: "); 	debugPort->println(data.avgMotorCurrent);
		debugPort->print("avgInputCurrent: "); 	debugPort->println(data.avgInputCurrent);
		debugPort->print("dutyCycleNow: "); 	debugPort->println(data.dutyCycleNow);
		debugPort->print("rpm: "); 				debugPort->println(data.rpm);
		debugPort->print("inputVoltage: "); 	debugPort->println(data.inpVoltage);
		debugPort->print("ampHours: "); 		debugPort->println(data.ampHours);
		debugPort->print("ampHoursCharges: "); 	debugPort->println(data.ampHoursCharged);
		debugPort->print("tachometer: "); 		debugPort->println(data.tachometer);
		debugPort->print("tachometerAbs: "); 	debugPort->println(data.tachometerAbs);
	}
}
