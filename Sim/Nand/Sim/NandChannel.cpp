#include "Nand/Sim/NandChannel.h"

void NandChannel::Init(BufferHal *bufferHal, U8 deviceCount, U32 blocksPerDevice, U32 pagesPerBlock, U32 bytesPerPage)
{
	for (U8 i(0); i < deviceCount; ++i)
	{
		_Devices.push_back(std::move(NandDevice(bufferHal, blocksPerDevice, pagesPerBlock, bytesPerPage)));
	}
}

NandDevice& NandChannel::operator[](const int index)
{
	return _Devices[index];
}