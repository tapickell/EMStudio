#include "freeemscomms.h"
#include <QDebug>
FreeEmsComms::FreeEmsComms(QObject *parent) : QThread(parent)
{
}
void FreeEmsComms::setPort(QString portname)
{
	m_portName = portname;
}

void FreeEmsComms::setBaud(int baudrate)
{
	m_baud = baudrate;
}

void FreeEmsComms::run()
{
	if (openPort(m_portName,m_baud))
	{
		qDebug() << "Error opening com port";
		return;
	}
	unsigned char buffer[1024];
	int readlen = 0;
	QByteArray qbuffer;
	bool inpacket= false;
	bool inescape=true;
	while (true)
	{
		readlen = read(m_portHandle,buffer,1024);
		for (int i=0;i<readlen;i++)
		{
			if (buffer[i] == 0xAA)
			{
				qDebug() << "Start of packet";
				//Start of packet
				inpacket = true;
			}
			if (buffer[i] == 0xCC)
			{
				qDebug() << "End of packet. Size:" << qbuffer.size();
				//End of packet
				inpacket = false;
				parseBuffer(qbuffer);
				qbuffer.clear();
			}
			else
			{
				if (inpacket && !inescape)
				{
					if (buffer[i] == 0xBB)
					{
						//Need to escape the next byte
						//retval = logfile.read(1);
						inescape = true;
					}
					else
					{
						qbuffer.append(buffer[i]);
					}

				}
				else if (inpacket && inescape)
				{
					if (buffer[i] == 0x55)
					{
						qbuffer.append((char)0xAA);
					}
					else if (buffer[i] == 0x44)
					{
						qbuffer.append((char)0xBB);
					}
					else if (buffer[i] == 0x33)
					{
						qbuffer.append((char)0xCC);
					}
					inescape = false;
				}
			}
		}
	}
}
void FreeEmsComms::parseBuffer(QByteArray buffer)
{
	if (buffer.size() <= 3)
	{
		qDebug() << "Not long enough to even contain a header!";
		return;
	}
	QByteArray header;
	//currPacket.clear();
	//Parse the packet here
	int headersize = 3;
	int iloc = 0;
	bool seq = false;
	bool len = false;
	if (buffer[iloc] & 0b00000100)
	{
		//Has header
		seq = true;
		qDebug() << "Has seq";
		headersize += 1;
	}
	if (buffer[iloc] & 0b00000001)
	{
		//Has length
		len = true;
		qDebug() << "Has length";
		headersize += 2;
	}
	header = buffer.mid(0,headersize);
	iloc++;
	unsigned int payloadid = (unsigned int)buffer[iloc] << 8;

	payloadid += (unsigned char)buffer[iloc+1];
	//qDebug() << QString::number(payloadid,16);
	qDebug() << QString::number(buffer[iloc-1],16);
	qDebug() << QString::number(buffer[iloc],16);
	qDebug() << QString::number(buffer[iloc+1],16);
	qDebug() << QString::number(buffer[iloc+2],16);
	iloc += 2;
	//qDebug() << QString::number(payloadid,16);
	if (seq)
	{
		//qDebug() << "Sequence number" << QString::number(currPacket[iloc]);
		iloc += 1;
	}
	QByteArray payload;
	if (len)
	{
		unsigned int length = buffer[iloc] << 8;
		length += buffer[iloc+1];
		qDebug() << "Length:" << length;
		iloc += 2;
		//curr += length;
		payload.append(buffer.mid(iloc,length));
	}
	else
	{
		qDebug() << "Buffer length:" << buffer.length();
		qDebug() << "Attempted cut:" << buffer.length() - iloc;
		payload.append(buffer.mid(iloc),(buffer.length()-iloc) -1);
	}
	//Last byte of currPacket should be out checksum.
	unsigned char sum = 0;
	for (int i=0;i<header.size();i++)
	{
		sum += header[i];
	}
	for (int i=0;i<payload.size();i++)
	{
		sum += payload[i];
	}
	//qDebug() << "Payload sum:" << QString::number(sum);
	//qDebug() << "Checksum sum:" << QString::number((unsigned char)currPacket[currPacket.length()-1]);
	if (sum != (unsigned char)buffer[buffer.length()-1])
	{
		qDebug() << "BAD CHECKSUM!";
	}
	else
	{
		qDebug() << "Got full packet. Header length:" << header.length() << "Payload length:" << payload.length();
		emit payloadReceived(header,payload);
		/*for (int i=0;i<m_dataFieldList->size();i++)
		{
			//ui.tableWidget->item(i,1)->setText(QString::number(m_dataFieldList[i].getValue(&payload)));
		}*/
		//payload is our actual data.
		//unsigned int rpm = (payload[26] << 8) + payload[27];

		//qDebug() << "f" << f.getValue(&payload);
		//qDebug() << QString::number(rpm);
		//qDebug() << QString::number(((unsigned short)payload[8] << 8) + (unsigned short)payload[9]);
	}
}

int FreeEmsComms::openPort(QString portName,int baudrate)
{
	m_portHandle = open(portName.toAscii(),O_RDWR | O_NOCTTY | O_NDELAY);
	if (m_portHandle < 0)
	{
		//printf("Error opening Com: %s\n",portName);
		//debug(obdLib::DEBUG_ERROR,"Error opening com port %s",portName);

		return -1;
	}
	//printf("Com Port Opened %i\n",portHandle);
	//debug(obdLib::DEBUG_VERBOSE,"Com Port Opened %i",portHandle);
	fcntl(m_portHandle, F_SETFL, 0); //Set it to blocking. This is required? Wtf?
	//struct termios oldtio;
	struct termios newtio;
	//bzero(&newtio,sizeof(newtio));
	tcgetattr(m_portHandle,&newtio);
	long BAUD = B115200;
	switch (baudrate)
	{
		case 38400:
			BAUD = B38400;
			break;
		case 115200:
			BAUD  = B115200;
			break;
		case 19200:
			BAUD  = B19200;
			break;
		case 9600:
			BAUD  = B9600;
			break;
		case 4800:
			BAUD  = B4800;
			break;
		default:
			BAUD = B115200;
			break;
	}  //end of switch baud_rate
	if (strspn("/dev/pts",portName.toAscii()) >= 8)
	{
		//debug(obdLib::DEBUG_WARN,"PTS Detected... disabling baud rate selection on: %s",portName);
		//printf("PTS detected... disabling baud rate selection: %s\n",portName);
		baudrate = -1;
	}
	else
	{
	}

	newtio.c_cflag |= (CLOCAL | CREAD | PARENB | PARODD);
	newtio.c_lflag &= !(ICANON | ECHO | ECHOE | ISIG);
	newtio.c_oflag &= !(OPOST);
	newtio.c_cc[VMIN] = 0; // Min number of bytes to read
	newtio.c_cc[VTIME] = 100; //10 second read timeout
	if (baudrate != -1)
	{
		if(cfsetispeed(&newtio, BAUD))
		{
			//perror("cfsetispeed");
		}

		if(cfsetospeed(&newtio, BAUD))
		{
			//perror("cfsetospeed");
		}
		//debug(obdLib::DEBUG_VERBOSE,"Setting baud rate to %i on port %s\n",baudrate,portName);
	}
	tcsetattr(m_portHandle,TCSANOW,&newtio);
	return 0;
}
