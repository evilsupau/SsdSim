#include "CustomProtocolInterface.h"

#include "HostComm/Ipc/MessageServer.hpp"

MessageServer<CustomProtocolCommand> _MessageServer("SsdSimCustomProtocolServer");

bool CustomProtocolInterface::HasCommand()
{
    return _MessageServer.HasMessage();
}

CustomProtocolCommand* CustomProtocolInterface::GetCommand()
{
    Message<CustomProtocolCommand>* msg = _MessageServer.Pop();
    if (msg)
    {
        msg->_Data.CommandId = msg->Id();
        return &msg->_Data;
    }

    return nullptr;
}

U8* CustomProtocolInterface::GetBuffer(CustomProtocolCommand *command, U32 &bufferSizeInBytes)
{
    Message<CustomProtocolCommand>* msg = _MessageServer.GetMessage(command->CommandId);
    if (msg)
    {
        bufferSizeInBytes = msg->_PayloadSize;
        return (U8*)msg->_Payload;
    }

    bufferSizeInBytes = 0;
    return nullptr;
}

void CustomProtocolInterface::SubmitResponse(CustomProtocolCommand *command)
{
    Message<CustomProtocolCommand> *message = _MessageServer.GetMessage(command->CommandId);
    if (message->ExpectsResponse())
    {
        _MessageServer.PushResponse(message);
    }
    else
    {
        _MessageServer.DeallocateMessage(message);
    }
}