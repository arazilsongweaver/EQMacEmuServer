#ifndef _EQSTREAMFACTORY_H

#define _EQSTREAMFACTORY_H

#include <memory>
#include <queue>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "../common/eq_stream.h"
#include "../common/condition.h"
#include "../common/timeoutmgr.h"

class Timer;

using EQOldStreamIterator = std::map<std::pair<uint32, uint16>, std::shared_ptr<EQOldStream>>::iterator;

class RecvBuffer {
	private:
		bool isnew;
		uint32 length;
		std::unique_ptr<unsigned char[]> buffer;
		std::pair<unsigned long, unsigned short> streamkey;
		sockaddr_in from;

	public:
		RecvBuffer(bool isnew, uint32 len, const unsigned char* buf, std::pair<unsigned long, unsigned short> key, sockaddr_in& f) : isnew(isnew), length(len), streamkey(key), from(f) {
				buffer.reset(new unsigned char[len]);
				memcpy(buffer.get(), buf, len);
		}

		bool IsNew() const { return isnew; }
		unsigned char* Buffer() const { return buffer.get(); }
		uint32 Length() const { return length; }
		const std::pair<unsigned long, unsigned short>& StreamKey() const { return streamkey; }
		const sockaddr_in& From() const { return from; }
};

class EQStreamFactory : private Timeoutable {
	private:
		int sock;
		int Port;

		bool ReaderRunning;
		std::mutex MReaderRunning;
		bool WriterRunningOld;
		std::mutex MWriterRunningOld;

		std::condition_variable WriterWorkOld;

		std::mutex MNewOldStreams;

		std::mutex MOldStreams;

		std::queue<std::shared_ptr<EQOldStream>> NewOldStreams;

		std::map<std::pair<uint32, uint16>, std::shared_ptr<EQOldStream>> OldStreams;

		std::thread ReaderThread;
		std::thread WriterOldThread;

		virtual void CheckTimeout();

		uint32 stream_timeout;

	public:
		EQStreamFactory() : Timeoutable(5000), stream_timeout(61000)
		{
			ReaderRunning = false;
			WriterRunningOld = false;
			Port = 0;
			sock = -1;
		}
		EQStreamFactory(int port, uint32 timeout = 61000);

		std::shared_ptr<EQOldStream> PopOld();
		void PushOld(std::shared_ptr<EQOldStream> s);

		bool Open();
		bool Open(unsigned long port) { Port=port; return Open(); }
		bool IsOpen() { return sock!=-1; }
		void Close();
		void ReaderLoop();
		void ProcessLoopOld(const RecvBuffer& recvBuffer, EQOldStreamIterator iterator);
		void WriterLoopOld();
		void Stop();
		void StopReader();
		void StopWriterOld();
		void SignalWriterOld();
};

#endif
