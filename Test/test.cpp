#include "pch.h"

#include <future>
#include <array>
#include <chrono>

#include "gtest-cout.h"
#include "Nand/NandDevice.h"
#include "Nand/NandHal.h"
#include "Framework.h"
#include "Ipc/MessageClient.h"

TEST(NandDeviceTest, Basic) {
	constexpr U32 blockCount = 64;
	constexpr U32 pagesPerBlock = 256;
	constexpr U32 bytesPerPage = 8192;

	NandDevice nandDevice(blockCount, pagesPerBlock, bytesPerPage);

	U8 pDataToWrite[bytesPerPage];
	for (auto i(0); i < sizeof(pDataToWrite); ++i)
	{
		pDataToWrite[i] = i % 255;
	}
	U8 pDataRead[bytesPerPage];
	U8 pErasedBuffer[bytesPerPage];
	std::memset(pErasedBuffer, NandBlock::ERASED_PATTERN, sizeof(pErasedBuffer));

	tBlockInDevice block;
	block._ = 0;
	for ( ; block._ < blockCount; ++block._)
	{
		tPageInBlock page;
		page._ = 0;
		for (; page._ < pagesPerBlock; ++page._)
		{
			nandDevice.WritePage(block, page, pDataToWrite);
			
			nandDevice.ReadPage(block, page, pDataRead);
			auto result = std::memcmp(pDataToWrite, pDataRead, bytesPerPage);
			ASSERT_EQ(0, result);
		}

		nandDevice.EraseBlock(block);
		for (; page._ < pagesPerBlock; ++page._)
		{
			nandDevice.ReadPage(block, page, pDataRead);
			auto result = std::memcmp(pErasedBuffer, pDataRead, bytesPerPage);
			ASSERT_EQ(0, result);
		}
	}
}

TEST(NandHalTest, Basic) {
	constexpr U8 channels = 4;
	constexpr U8 devices = 2;
	constexpr U32 blocks = 64;
	constexpr U32 pages = 256;
	constexpr U32 bytes = 8192;

	NandHal nandHal;
	nandHal.PreInit(channels, devices, blocks, pages, bytes);
	nandHal.Init();

	U8 pDataToWrite[bytes];
	for (auto i(0); i < sizeof(pDataToWrite); ++i)
	{
		pDataToWrite[i] = i % 255;
	}
	U8 pDataRead[bytes];
	U8 pErasedBuffer[bytes];
	std::memset(pErasedBuffer, NandBlock::ERASED_PATTERN, sizeof(pErasedBuffer));

	tChannel channel;
	channel._ = 0;
	for (; channel._ < channels; ++channel._)
	{
		tDeviceInChannel device;
		device._ = 0;
		for (; device._ < devices; ++device._)
		{
			tBlockInDevice block;
			block._ = 0;
			for (; block._ < blocks; ++block._)
			{
				tPageInBlock page;
				page._ = 0;
				for (; page._ < pages; ++page._)
				{
					nandHal.WritePage(channel, device, block, page, pDataToWrite);

					nandHal.ReadPage(channel, device, block, page, pDataRead);
					auto result = std::memcmp(pDataToWrite, pDataRead, bytes);
					ASSERT_EQ(0, result);
				}

				nandHal.EraseBlock(channel, device, block);
				for (; page._ < pages; ++page._)
				{
					nandHal.ReadPage(channel, device, block, page, pDataRead);
					auto result = std::memcmp(pErasedBuffer, pDataRead, bytes);
					ASSERT_EQ(0, result);
				}
			}
		}
	}
}

TEST(NandHalTest, BasicCommandQueue)
{
	constexpr U8 channels = 4;
	constexpr U8 devices = 2;
	constexpr U32 blocks = 64;
	constexpr U32 pages = 256;
	constexpr U32 bytes = 8192;
	constexpr U8 bufferCount = channels * 4;
	constexpr U32 commandCount = channels * devices * blocks * pages;

	std::array<U8[bytes], bufferCount> writeBuffers;
	for (auto buffer : writeBuffers)
	{
		for (auto i(0); i < bytes; ++i)
		{
			buffer[i] = i % 255;
		}
	}

	std::array<U8[bytes], bufferCount> readBuffers;

	U8 pErasedBuffer[bytes];
	std::memset(pErasedBuffer, NandBlock::ERASED_PATTERN, sizeof(pErasedBuffer));
	
	NandHal nandHal;
	nandHal.PreInit(channels, devices, blocks, pages, bytes);
	nandHal.Init();

	std::future<void> nandHalFuture;

	nandHalFuture = std::async(std::launch::async, &NandHal::operator(), &nandHal);

	NandHal::CommandDesc commandDesc;
	commandDesc.Channel._ = 0;
	commandDesc.Device._ = 0;
	commandDesc.Block._ = 0;
	commandDesc.Page._ = 0;
	for (auto c(0); c < commandCount;)
	{
		for (auto i(0); i < bufferCount; ++i, ++c)
		{
			commandDesc.Operation = NandHal::CommandDesc::Op::WRITE;
			commandDesc.Buffer = writeBuffers[i];
			nandHal.QueueCommand(commandDesc);

			commandDesc.Operation = NandHal::CommandDesc::Op::READ;
			commandDesc.Buffer = readBuffers[i];
			nandHal.QueueCommand(commandDesc);

			if (++commandDesc.Channel._ >= channels)
			{
				commandDesc.Channel._ = 0;
				if (++commandDesc.Device._ >= devices)
				{
					commandDesc.Device._ = 0;
					if (++commandDesc.Page._ >= pages)
					{
						commandDesc.Page._ = 0;
						assert(commandDesc.Block._ < blocks);	//let's not go pass this boundary
						++commandDesc.Block._;
					}
				}
			}
		}

		while (false == nandHal.IsCommandQueueEmpty());
	}

	nandHal.Stop();
}

TEST(SimFramework, Basic)
{
	Framework framework;
	auto fwFuture = std::async(std::launch::async, &(Framework::operator()), &framework);

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::shared_ptr<MessageClient> client = std::make_shared<MessageClient>(SSDSIM_IPC_NAME);
    ASSERT_NE(nullptr, client);

    Message *message = client->AllocateMessage(Message::Type::Exit);
    client->Push(message);
}

TEST(SimFramework, Benchmark)
{
    constexpr U32 loopCount = 10000;

    Framework framework;
    auto fwFuture = std::async(std::launch::async, &(Framework::operator()), &framework);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::shared_ptr<MessageClient> client = std::make_shared<MessageClient>(SSDSIM_IPC_NAME);
    ASSERT_NE(nullptr, client);

    using namespace std::chrono;
    milliseconds startMs = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    for (U32 i = 0; i < loopCount; ++i)
    {
        Message *message = client->AllocateMessage(Message::Type::NOP);
        client->Push(message);
    }
    milliseconds endMs = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

    while (loopCount != framework._NopCount)
    {
        // Do nothing
    }

    GOUT(loopCount << " NOPs took " << (endMs.count() - startMs.count()) << " ms to process");

    Message *message = client->AllocateMessage(Message::Type::Exit);
    client->Push(message);
}

TEST(MessagingSystem, MessageWithPayload)
{
    constexpr char name[] = "Test";
    constexpr U32 size = 10 * 1024 * 1024;
    constexpr U32 loopCount = 10;
    constexpr U32 payloadSize = 100;

    std::shared_ptr<MessageServer> server = std::make_shared<MessageServer>(name, size);
    ASSERT_NE(nullptr, server);

    std::shared_ptr<MessageClient> client = std::make_shared<MessageClient>(name);
    ASSERT_NE(nullptr, client);

    U8 count;

    // Send message
    count = 0;
    for (U32 i = 0; i < loopCount; ++i)
    {
        Message *message = client->AllocateMessage(Message::Type::NOP, payloadSize);
        ASSERT_NE(nullptr, message);

        U8* payload = (U8*)message->_Payload;
        for (U32 j = 0; j < payloadSize; ++j)
        {
            payload[j] = count;
            ++count;
        }
        client->Push(message);
    }

    // Verify
    count = 0;
    for (U32 i = 0; i < loopCount; ++i)
    {
        if (server->HasMessage())
        {
            Message *message = server->Pop();
            ASSERT_EQ(Message::Type::NOP, message->_Type);
            ASSERT_EQ(payloadSize, message->_PayloadSize);

            U8* payload = (U8*)message->_Payload;
            for (U32 j = 0; j < payloadSize; ++j)
            {
                ASSERT_EQ(count, payload[j]);
                ++count;
            }
            server->DeallocateMessage(message);
        }
        else
        {
            ASSERT_FALSE(true);
        }
    }
}