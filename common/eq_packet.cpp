/*
	Copyright (C) 2005 Michael S. Finger

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; version 2 of the License.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY except by those people which sell it, which
	are required to give you total support for your newly bought product;
	without even the implied warranty of MERCHANTABILITY or FITNESS FOR
	A PARTICULAR PURPOSE. See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "global_define.h"
#include "eq_packet.h"
#include "misc.h"
#include "platform.h"

#include <sstream>
#ifndef STATIC_OPCODE
#include "opcodemgr.h"
#endif
#include "packet_dump.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdio.h>

EQPacket::EQPacket(EmuOpcode op, const unsigned char *buf, uint32 len)
:	BasePacket(buf, len),
	emu_opcode(op)
{
}

void EQPacket::build_raw_header_dump(char *buffer, uint16 seq) const {
	BasePacket::build_raw_header_dump(buffer, seq);
	buffer += strlen(buffer);

	buffer += sprintf(buffer, "[EmuOpCode 0x%04x Size=%u]\n", emu_opcode, size);
}

void EQPacket::DumpRawHeader(uint16 seq, FILE *to) const
{
	char buff[196];
	build_raw_header_dump(buff, seq);
	fprintf(to, "%s", buff);
}

void EQPacket::build_header_dump(char *buffer) const {
	sprintf(buffer, "[EmuOpCode 0x%04x Size=%u]", emu_opcode, size);
}

void EQPacket::DumpRawHeaderNoTime(uint16 seq, FILE *to) const
{
	if (src_ip) {
		std::string sIP,dIP;
		sIP=long2ip(src_ip);
		dIP=long2ip(dst_ip);
		fprintf(to, "[%s:%d->%s:%d] ",sIP.c_str(),src_port,dIP.c_str(),dst_port);
	}
	if (seq != 0xffff)
		fprintf(to, "[Seq=%u] ",seq);

	fprintf(to, "[EmuOpCode 0x%04x Size=%lu]\n",emu_opcode,(unsigned long)size);
}

void EQApplicationPacket::build_raw_header_dump(char *buffer, uint16 seq) const
{
	BasePacket::build_raw_header_dump(buffer, seq);
	buffer += strlen(buffer);

#ifdef STATIC_OPCODE
	buffer += sprintf(buffer, "[OpCode 0x%04x Size=%u]\n", emu_opcode,size);
#else
	buffer += sprintf(buffer, "[OpCode %s Size=%u]\n",OpcodeManager::EmuToName(emu_opcode),size);
#endif
}

void EQApplicationPacket::DumpRawHeader(uint16 seq, FILE *to) const
{
	char buff[196];
	build_raw_header_dump(buff, seq);
	fprintf(to, "%s", buff);
}

void EQApplicationPacket::build_header_dump(char *buffer) const
{
#ifdef STATIC_OPCODE
	sprintf(buffer, "[OpCode 0x%04x Size=%u]\n", emu_opcode,size);
#else
	sprintf(buffer, "[OpCode %s(0x%04x) Size=%u]", OpcodeManager::EmuToName(emu_opcode), GetProtocolOpcode(), size);
#endif
}

void EQApplicationPacket::DumpRawHeaderNoTime(uint16 seq, FILE *to) const
{
	if (src_ip) {
		std::string sIP,dIP;
		sIP=long2ip(src_ip);
		dIP=long2ip(dst_ip);
		fprintf(to, "[%s:%d->%s:%d] ",sIP.c_str(),src_port,dIP.c_str(),dst_port);
	}
	if (seq != 0xffff)
		fprintf(to, "[Seq=%u] ",seq);

#ifdef STATIC_OPCODE
	fprintf(to, "[OpCode 0x%04x Size=%u]\n", emu_opcode,size);
#else
	fprintf(to, "[OpCode %s Size=%lu]\n",OpcodeManager::EmuToName(emu_opcode),(unsigned long)size);
#endif
}

/*
this is the code to do app-layer combining, instead of protocol layer.
this was taken out due to complex interactions with the opcode manager,
and will require a bit more thinking (likely moving into EQStream) to
get running again... but might be a good thing some day.

bool EQApplicationPacket::combine(const EQApplicationPacket *rhs)
{
uint32 newsize=0, offset=0;
unsigned char *tmpbuffer=nullptr;

	if (opcode!=OP_AppCombined) {
		newsize=app_opcode_size+size+(size>254?3:1)+app_opcode_size+rhs->size+(rhs->size>254?3:1);
		tmpbuffer=new unsigned char [newsize];
		offset=0;
		if (size>254) {
			tmpbuffer[offset++]=0xff;
			*(uint16 *)(tmpbuffer+offset)=htons(size);
			offset+=1;
		} else {
			tmpbuffer[offset++]=size;
		}
		offset+=serialize(tmpbuffer+offset);
	} else {
		newsize=size+app_opcode_size+rhs->size+(rhs->size>254?3:1);
		tmpbuffer=new unsigned char [newsize];
		memcpy(tmpbuffer,pBuffer,size);
		offset=size;
	}

	if (rhs->size>254) {
		tmpbuffer[offset++]=0xff;
		*(uint16 *)(tmpbuffer+offset)=htons(rhs->size);
		offset+=1;
	} else {
		tmpbuffer[offset++]=rhs->size;
	}
	offset+=rhs->serialize(tmpbuffer+offset);

	size=offset;
	opcode=OP_AppCombined;

	delete[] pBuffer;
	pBuffer=tmpbuffer;

	return true;
}
*/

EQApplicationPacket *EQApplicationPacket::Copy() const {
	return(new EQApplicationPacket(*this));
}

void DumpPacket(const EQApplicationPacket* app, bool iShowInfo) 
{
	#if EQDEBUG < 11
		return;
	#endif

	if (iShowInfo) {
		std::cout << "Dumping Applayer: 0x" << std::hex << std::setfill('0') << std::setw(4) << app->GetOpcode() << std::dec;
		std::cout << " size:" << app->size << std::endl;
	}
	DumpPacketHex(app->pBuffer, app->size);
//	DumpPacketAscii(app->pBuffer, app->size);
}

std::string DumpPacketToString(const EQApplicationPacket* app){
	std::ostringstream out;
	out << DumpPacketHexToString(app->pBuffer, app->size);
	return out.str();
}
