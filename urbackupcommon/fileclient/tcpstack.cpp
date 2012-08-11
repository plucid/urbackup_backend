/*************************************************************************
*    UrBackup - Client/Server backup system
*    Copyright (C) 2011  Martin Raiber
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "tcpstack.h"
#include "data.h"
#include "../../md5.h"
#include <memory.h>

#define SEND_TIMEOUT 10000
#define MAX_PACKET 4096

const unsigned int checksum_len=16;

CTCPStack::CTCPStack(bool add_checksum)
	: add_checksum(add_checksum)
{
}

void CTCPStack::AddData(char* buf, size_t datasize)
{
	if(datasize>0)
	{
		size_t osize=buffer.size();
		buffer.resize(osize+datasize);
		memcpy(&buffer[osize], buf, datasize);
	}
}

size_t CTCPStack::Send(IPipe* p, char* buf, size_t msglen)
{
	char* buffer;
	size_t msg_off=sizeof(MAX_PACKETSIZE);
	size_t len_off=0;
	size_t packet_len=msglen+sizeof(MAX_PACKETSIZE);
	if(!add_checksum)
	{
		buffer=new char[packet_len];
	}
	else
	{
		packet_len=checksum_len+msglen+sizeof(MAX_PACKETSIZE);
		buffer=new char[packet_len];
		msg_off=checksum_len+sizeof(MAX_PACKETSIZE);
		len_off=checksum_len;
	}

	MAX_PACKETSIZE len=(MAX_PACKETSIZE)msglen;

	memcpy(&buffer[len_off], &len, sizeof(MAX_PACKETSIZE) );
	if(msglen>0)
	{
	    memcpy(&buffer[msg_off], buf, msglen);
	}

	if(add_checksum)
	{
		MD5 md((unsigned char*)&buffer[len_off], (unsigned int)msglen+sizeof(MAX_PACKETSIZE));
		memcpy(buffer, md.raw_digest_int(), checksum_len);
	}

	size_t currpos=0;

	while(currpos<packet_len)
	{
		size_t ts=(std::min)((size_t)MAX_PACKET, packet_len-currpos);
		bool b=p->Write(&buffer[currpos], ts, SEND_TIMEOUT );
		currpos+=ts;
		if(!b)
		{
			delete[] buffer;
			return 0;
		}
	}
	delete[] buffer;

	return msglen;
}

size_t  CTCPStack::Send(IPipe* p, CWData data)
{
	return Send(p, data.getDataPtr(), data.getDataSize() );
}

size_t CTCPStack::Send(IPipe* p, const std::string &msg)
{
	return Send(p, (char*)msg.c_str(), msg.size());
}


char* CTCPStack::getPacket(size_t* packetsize)
{
	if(!add_checksum)
	{
		if(buffer.size()>=sizeof(MAX_PACKETSIZE))
		{
			MAX_PACKETSIZE len;
			memcpy(&len, &buffer[0], sizeof(MAX_PACKETSIZE) );

			if(buffer.size()>=(size_t)len+sizeof(MAX_PACKETSIZE))
			{
				char* buf=new char[len+1];
				if(len>0)
				{
					memcpy(buf, &buffer[sizeof(MAX_PACKETSIZE)], len);
				}

				(*packetsize)=len;

				buffer.erase(buffer.begin(), buffer.begin()+len+sizeof(MAX_PACKETSIZE));

				buf[len]=0;

				return buf;
			}
		}
	}
	else
	{
		if(buffer.size()>=sizeof(MAX_PACKETSIZE)+checksum_len)
		{
			MAX_PACKETSIZE len;
			memcpy(&len, &buffer[checksum_len], sizeof(MAX_PACKETSIZE) );

			if(buffer.size()>=checksum_len+(size_t)len+sizeof(MAX_PACKETSIZE))
			{
				MD5 md((unsigned char*)&buffer[checksum_len], (size_t)len+sizeof(MAX_PACKETSIZE) );

				if(memcmp(md.raw_digest_int(), &buffer[0], checksum_len)==0)
				{
					char* buf=new char[len+1];
					if(len>0)
					{
						memcpy(buf, &buffer[checksum_len+sizeof(MAX_PACKETSIZE)], len);
					}

					(*packetsize)=len;

					buffer.erase(buffer.begin(), buffer.begin()+checksum_len+len+sizeof(MAX_PACKETSIZE));

					buf[len]=0;

					return buf;
				}
				else
				{
					buffer.erase(buffer.begin(), buffer.begin()+checksum_len+len+sizeof(MAX_PACKETSIZE));
				}
			}
		}
	}
	return NULL;
}

void CTCPStack::reset(void)
{
        buffer.clear();
}

char *CTCPStack::getBuffer()
{
	return &buffer[0];
}

size_t CTCPStack::getBuffersize()
{
	return buffer.size();
}

void CTCPStack::setAddChecksum(bool b)
{
	add_checksum=b;
}