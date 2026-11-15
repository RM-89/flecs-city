#pragma once

namespace fc::ECS
{

/// @brief A single serialised ECS message sent over the Messaging channel.
///
/// Message format: [uint32 typeHash][uint16 payloadSize][payload bytes].
/// The type hash identifies the message component, and the payload is the raw (trivially copyable) component data.
/// Peer pointers are resolved locally on each side (@c mRecipient on send, @c mSender on receive).
struct Message
{
    uint32_t mTypeHash{0};
    std::vector<uint8_t> mData;

    /// @brief Service-side only. Either the peer to send to, or nullptr to broadcast.
    ENetPeer* mRecipient{nullptr};

    /// @brief Receive-side only. The peer the message came from.
    ENetPeer* mSender{nullptr};

    static constexpr size_t HEADER_SIZE = sizeof(uint32_t) + sizeof(uint16_t);

    [[nodiscard]] std::vector<uint8_t> Serialise() const
    {
        std::vector<uint8_t> buffer;
        buffer.reserve(HEADER_SIZE + mData.size());

        auto Write = [&buffer](const void* data, const size_t size) {
            const auto* bytes = static_cast<const uint8_t*>(data);
            buffer.insert(buffer.end(), bytes, bytes + size);
        };

        Write(&mTypeHash, sizeof(mTypeHash));

        const auto dataSize = static_cast<uint16_t>(mData.size());
        Write(&dataSize, sizeof(dataSize));

        buffer.insert(buffer.end(), mData.begin(), mData.end());

        return buffer;
    }

    /// @brief Deserialises a message packet.
    /// @return True on success; false if the buffer is truncated or malformed.
    static bool Deserialise(const uint8_t* data, const size_t size, Message& outMessage)
    {
        if (data == nullptr || size < HEADER_SIZE)
        {
            return false;
        }

        uint32_t typeHash;
        uint16_t payloadSize;
        memcpy(&typeHash, data, sizeof(typeHash));
        memcpy(&payloadSize, data + sizeof(typeHash), sizeof(payloadSize));

        // The packet must be exactly the header plus the declared payload
        if (size != HEADER_SIZE + payloadSize)
        {
            return false;
        }

        outMessage.mTypeHash = typeHash;
        outMessage.mData.assign(data + HEADER_SIZE, data + HEADER_SIZE + payloadSize);

        return true;
    }
};

} // namespace fc::ECS