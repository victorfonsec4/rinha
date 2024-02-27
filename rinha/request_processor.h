#ifndef RINHA_REQUEST_PROCESSOR_H
#define RINHA_REQUEST_PROCESSOR_H

#include <unistd.h>
#include <vector>

namespace rinha {

void ProcessRequest(std::vector<char> *buffer_p, ssize_t num_read,
                    int client_fd);

} // namespace rinha

#endif
