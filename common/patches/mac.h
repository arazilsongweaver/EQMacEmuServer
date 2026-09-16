#ifndef MAC_H_
#define MAC_H_

class EQApplicationPacket;
class EQPacketEncodeResult;
class EQPacketTranslator;

namespace Mac {

	// Register the Mac packet encoders and decoders with the application packet translator.
	extern void Register(EQPacketTranslator &translator);

	class Strategy {
	public:
		void Register(EQPacketTranslator &translator) const;

	private:
		//magic macro to declare our opcodes
		#include "ss_declare.h"
		#include "mac_ops.h"


	};

};



#endif /*MAC_H_*/
