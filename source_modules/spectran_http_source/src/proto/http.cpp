#include "http.h"
#include <inttypes.h>

namespace net::http {
    std::string MessageHeader::serialize() {
        std::string data;

        // Add start line
        data += serializeStartLine() + "\r\n";

        // Add fields
        for (const auto& [key, value] : fields) {
            data += key + ": " + value + "\r\n";
        }

        // Add terminator
        data += "\r\n";

        return data;
    }

    void MessageHeader::deserialize(const std::string& data) {
        // Clear existing fields
        fields.clear();

        // Parse first line
        std::string line;
        int offset = readLine(data, line);
        deserializeStartLine(line);

        // Parse fields
        while (offset < data.size()) {
            // Read line
            offset = readLine(data, line, offset);

            // If empty line, the header is done
            if (line.empty()) { break; }

            // Read until first ':' for the key
            int klen = 0;
            for (; klen < line.size(); klen++) {
                if (line[klen] == ':') { break; }
            }
            
            // Find offset of value
            int voff = klen + 1;
            for (; voff < line.size(); voff++) {
                if (line[voff] != ' ' && line[voff] != '\t') { break; }
            }

            // Save field
            fields[line.substr(0, klen)] = line.substr(voff);
        }
    }

    std::map<std::string, std::string>& MessageHeader::getFields() {
        return fields;
    }

    bool MessageHeader::hasField(const std::string& name) {
        return fields.find(name) != fields.end();
    }

    std::string MessageHeader::getField(const std::string name) {
        // TODO: Check if exists
        return fields[name];
    }

    void MessageHeader::setField(const std::string& name, const std::string& value) {
        fields[name] = value;
    }

    void MessageHeader::clearField(const std::string& name) {
        // TODO: Check if exists (but maybe no error?)
        fields.erase(name);
    }

    int MessageHeader::readLine(const std::string& str, std::string& line, int start) {
        // Get line length
        int len = 0;
        bool cr = false;
        for (int i = start; i < str.size(); i++) {
            if (str[i] == '\n') {
                if (len && str[i-1] == '\r') { cr = true; }
                break;
            }
            len++;
        }

        // Copy line
        line = str.substr(start, len - (cr ? 1:0));
        return start + len + 1;
    }

    RequestHeader::RequestHeader(Method method, std::string uri, std::string host) {
        this->method = method;
        this->uri = uri;
        setField("Host", host);
    }

    RequestHeader::RequestHeader(const std::string& data) {
        deserialize(data);
    }

    Method RequestHeader::getMethod() {
        return method;
    }

    void RequestHeader::setMethod(Method method) {
        this->method = method;
    }

    std::string RequestHeader::getURI() {
        return uri;
    }

    void RequestHeader::setURI(const std::string& uri) {
        this->uri = uri;
    }

    void RequestHeader::deserializeStartLine(const std::string& data) {
        // TODO
    }

    std::string RequestHeader::serializeStartLine() {
        // TODO: Allow to specify version
        return MethodStrings[method] + " " + uri + " HTTP/1.1";
    }

    ResponseHeader::ResponseHeader(StatusCode statusCode) {
        this->statusCode = statusCode;
        if (StatusCodeStrings.find(statusCode) != StatusCodeStrings.end()) {
            this->statusString = StatusCodeStrings[statusCode];
        }
        else {
            this->statusString = "UNKNOWN";
        }
    }

    ResponseHeader::ResponseHeader(StatusCode statusCode, const std::string& statusString) {
        this->statusCode = statusCode;
        this->statusString = statusString;
    }

    ResponseHeader::ResponseHeader(const std::string& data) {
        deserialize(data);
    }

    StatusCode ResponseHeader::getStatusCode() {
        return statusCode;
    }

    void ResponseHeader::setStatusCode(StatusCode statusCode) {
        this->statusCode = statusCode;
    }

    std::string ResponseHeader::getStatusString() {
        return statusString;
    }

    void ResponseHeader::setStatusString(const std::string& statusString) {
        this->statusString = statusString;
    }

    void ResponseHeader::deserializeStartLine(const std::string& data) {
        // Parse version
        int offset = 0;
        for (; offset < data.size(); offset++) {
            if (data[offset] == ' ') { break; }
        }
        // TODO: Error if null length
        // TODO: Parse version

        // Skip spaces
        for (; offset < data.size(); offset++) {
            if (data[offset] != ' ' && data[offset] != '\t') { break; }
        }
        
        // Parse status code
        int codeOffset = offset;
        for (; offset < data.size(); offset++) {
            if (data[offset] == ' ') { break; }
        }
        // TODO: Error if null length
        statusCode = (StatusCode)std::stoi(data.substr(codeOffset, codeOffset - offset));
    
        // Skip spaces
        for (; offset < data.size(); offset++) {
            if (data[offset] != ' ' && data[offset] != '\t') { break; }
        }
        
        // Parse status string
        int stringOffset = offset;
        for (; offset < data.size(); offset++) {
            if (data[offset] == ' ') { break; }
        }
        // TODO: Error if null length (maybe?)
        statusString = data.substr(stringOffset, stringOffset - offset);
    }

    std::string ResponseHeader::serializeStartLine() {
        char buf[1024];
        sprintf(buf, "%d %s", (int)statusCode, statusString.c_str());
        return buf;
    }

    ChunkHeader::ChunkHeader(size_t length) {
        this->length = length;
    }

    ChunkHeader::ChunkHeader(const std::string& data) {
        deserialize(data);
    }

    std::string ChunkHeader::serialize() {
        char buf[64];
        sprintf(buf, "%" PRIX64 "\r\n", length);
        return buf;
    }

    void ChunkHeader::deserialize(const std::string& data) {
        // Parse length
        int offset = 0;
        for (; offset < data.size(); offset++) {
            if (data[offset] == ' ') { break; }
        }
        // TODO: Error if null length
        length = (StatusCode)std::stoull(data.substr(0, offset), nullptr, 16);

        // TODO: Parse rest
    }

    size_t ChunkHeader::getLength() {
        return length;
    }

    void ChunkHeader::setLength(size_t length) {
        this->length = length;
    }

    Client::Client(std::shared_ptr<Socket> sock) {
        this->sock = sock;
    }

    int Client::sendRequestHeader(RequestHeader& req) {
        return sock->sendstr(req.serialize());
    }

    int Client::recvRequestHeader(RequestHeader& req, int timeout) {
        // Non-blocking mode not alloowed
        if (!timeout) { return -1; }

        // Read response
        std::string respData;
        int err = recvHeader(respData, timeout);
        if (err) { return err; }

        // Deserialize
        req.deserialize(respData);
    }

    int Client::sendResponseHeader(ResponseHeader& resp) {
        return sock->sendstr(resp.serialize());
    }

    int Client::recvResponseHeader(ResponseHeader& resp, int timeout) {
        // Non-blocking mode not alloowed
        if (!timeout) { return -1; }

        // Read response
        std::string respData;
        int err = recvHeader(respData, timeout);
        if (err) { return err; }

        // Deserialize
        resp.deserialize(respData);
    }

    int Client::sendChunkHeader(ChunkHeader& chdr) {
        return sock->sendstr(chdr.serialize());
    }

    int Client::recvChunkHeader(ChunkHeader& chdr, int timeout) {
        std::string respData;
        int err = sock->recvline(respData, 0, timeout);
        if (err <= 0) { return err; }
        if (respData[respData.size()-1] == '\r') {
            respData.pop_back();
        }
        chdr.deserialize(respData);
        return 0;
    }

    int Client::recvHeader(std::string& data, int timeout) {
        while (sock->isOpen()) {
            std::string line;
            int ret = sock->recvline(line);
            if (line == "\r") { break; }
            if (ret <= 0) { return ret; }
            data += line + "\n";
        }
        return 0;
    }
}