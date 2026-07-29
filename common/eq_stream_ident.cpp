#include "global_define.h"
#include "eqemu_logsys.h"
#include "eq_stream_ident.h"
#include "eq_stream_proxy.h"


EQStreamIdentifier::~EQStreamIdentifier() {
	while(!m_identified.empty()) {
		m_identified.front()->ReleaseFromUse();
		m_identified.pop();
	}
	std::vector<OldRecord *>::iterator oldcur, oldend;
	oldcur = m_oldstreams.begin();
	oldend = m_oldstreams.end();
	for(; oldcur != oldend; ++oldcur) {
		OldRecord *r = *oldcur;
		if (r != nullptr)
			r->stream->ReleaseFromUse();
		safe_delete(r);
	}

	std::vector<OldPatch *>::iterator oldcurp, oldendp;
	oldcurp = m_oldpatches.begin();
	oldendp = m_oldpatches.end();
	for(; oldcurp != oldendp; ++oldcurp) {
		safe_delete(*oldcurp);
	}
}

void EQStreamIdentifier::RegisterOldPatch(const EQStreamInterface::Signature &sig, const char *name, OpcodeManager ** opcodes, const StructStrategy *structs) {
	OldPatch *p = new OldPatch;
	p->signature = sig;
	p->name = name;
	p->opcodes = opcodes;
	p->structs = structs;
	m_oldpatches.push_back(p);
}


void EQStreamIdentifier::Process() {
	std::vector<OldRecord *>::iterator oldcur;
	std::vector<OldPatch *>::iterator oldcurp, oldendp;

	//foreach pending old stream.
	oldcur = m_oldstreams.begin();
	while(oldcur != m_oldstreams.end()) {
		OldRecord *r = *oldcur;

		//first see if this stream has expired
		if(r != nullptr && r->expire.Check(false)) {
			//this stream has failed to match any pattern in our timeframe.
			LogNetcodeDetail("Unable to identify stream from [{}]:[{}] before timeout.", long2ip(r->stream->GetRemoteIP()).c_str(), ntohs(r->stream->GetRemotePort()));
			r->stream->ReleaseFromUse();
			safe_delete(r);
			oldcur = m_oldstreams.erase(oldcur);
			continue;
		}

		//then make sure the stream is still active
		//if stream hasn't finished initializing then continue;
		if (r != nullptr && r->stream->GetState() == UNESTABLISHED)
		{
			continue;
		}
		if (r != nullptr && r->stream->GetState() != ESTABLISHED) {
			//the stream closed before it was identified.
			LogNetcodeDetail("Unable to identify stream from [{}]:[{}] before it closed.", long2ip(r->stream->GetRemoteIP()).c_str(), ntohs(r->stream->GetRemotePort()));
			switch(r->stream->GetState())
			{
			case ESTABLISHED:
				LogNetcodeDetail("Stream state was Established");
				break;
			case CLOSING:
				LogNetcodeDetail("Stream state was Closing");
				break;
			case DISCONNECTING:
				LogNetcodeDetail("Stream state was Disconnecting");
				break;
			case CLOSED:
				LogNetcodeDetail("Stream state was Closed");
				break;
			default:
				LogNetcodeDetail("Stream state was Unestablished or unknown");
				break;
			}
			r->stream->ReleaseFromUse();
			safe_delete(r);
			oldcur = m_oldstreams.erase(oldcur);
			continue;
		}

		//not expired, check against all patch signatures

		bool found_one = false;		//"we found a matching patch for this stream"
		bool all_ready = true;		//"all signatures were ready to check the stream"

		//foreach possbile patch...
		oldcurp = m_oldpatches.begin();
		oldendp = m_oldpatches.end();
		for(; !found_one && oldcurp != oldendp; ++oldcurp) {
			OldPatch *p = *oldcurp;

			//ask the stream to see if it matches the supplied signature
			if (r)
			{
				EQStreamInterface::MatchState res = r->stream->CheckSignature(&p->signature);
				switch (res) {
				case EQStreamInterface::MatchNotReady:
					//the stream has not received enough packets to compare with this signature
					LogNetcodeDetail("[{}]:[{}]: Tried patch {}, but stream is not ready for it.", long2ip(r->stream->GetRemoteIP()), ntohs(r->stream->GetRemotePort()), p->name);
					all_ready = false;
					break;
				case EQStreamInterface::MatchSuccessful: {
					//yay, a match.

					LogNetcodeDetail("Identified stream [{}]:[{}] with signature {}", long2ip(r->stream->GetRemoteIP()), ntohs(r->stream->GetRemotePort()), p->name);


					//might want to do something less-specific here... some day..
					EQStreamInterface *s = new EQStreamProxy(r->stream, p->structs, p->opcodes);
					m_identified.push(s);

					found_one = true;
					break;
				}
				case EQStreamInterface::MatchFailed:
					//do nothing...
					LogNetcodeDetail("[{}]:[{}]: Tried patch {}, and it did not match.", long2ip(r->stream->GetRemoteIP()), ntohs(r->stream->GetRemotePort()), p->name);
					break;
				}
			}
		}

		//if we checked all patches and did not find a match.
		if(all_ready && !found_one) {
			//the stream cannot be identified.
			if (r)
			{
				LogNetcodeDetail("Unable to identify stream from [{}]:[{}], no match found.", long2ip(r->stream->GetRemoteIP()).c_str(), ntohs(r->stream->GetRemotePort()));
				r->stream->ReleaseFromUse();
			}
		}

		//if we found a match, or were not able to identify it
		if(found_one || all_ready) {
			//cannot print ip/port here. r->stream is invalid.
			safe_delete(r);
			oldcur = m_oldstreams.erase(oldcur);
		} else {
			++oldcur;
		}
	}	//end foreach stream
}

void EQStreamIdentifier::AddOldStream(std::shared_ptr<EQOldStream> eqs) {
	m_oldstreams.emplace_back(new OldRecord(eqs));
	eqs = nullptr;
}

EQStreamInterface *EQStreamIdentifier::PopIdentified() {
	if(m_identified.empty())
		return(nullptr);
	EQStreamInterface *res = m_identified.front();
	m_identified.pop();
	return(res);
}

EQStreamIdentifier::OldRecord::OldRecord(std::shared_ptr<EQOldStream> s)
:	stream(s),
	expire(STREAM_IDENT_WAIT_MS)
{
	expire.Start();
}


