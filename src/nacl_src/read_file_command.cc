#include <cstdio>

#include "ppapi/cpp/var_array_buffer.h"
#include "read_file_command.h"

ReadFileCommand::ReadFileCommand(SftpEventListener *listener,
                                 const int server_sock,
                                 LIBSSH2_SESSION *session,
                                 LIBSSH2_SFTP *sftp_session,
                                 const int request_id,
                                 const std::string &path,
                                 const libssh2_uint64_t offset,
                                 const libssh2_uint64_t length,
                                 const unsigned int buffer_size)
: AbstractCommand(session, sftp_session, server_sock, listener, request_id),
  path_(path),
  offset_(offset),
  length_(length),
  buffer_size_(buffer_size)
{
  fprintf(stderr, "ReadFileCommand::ReadFileCommand\n");
}

ReadFileCommand::~ReadFileCommand()
{
  fprintf(stderr, "ReadFileCommand::~ReadFileCommand\n");
}

void* ReadFileCommand::Start(void *arg)
{
  ReadFileCommand *instance = static_cast<ReadFileCommand*>(arg);
  instance->Execute();
  return NULL;
}

void ReadFileCommand::Execute()
{
  fprintf(stderr, "ReadFileCommand::Execute\n");
  try {
    LIBSSH2_SFTP_HANDLE *sftp_handle = OpenFile(path_, LIBSSH2_FXF_READ, 0);
    SeekAtOffsetOf(sftp_handle, offset_);
    ReadFileLengthOf(sftp_handle, length_, buffer_size_);
    // The sftp_handle will beclosed in the ReadFileLengthOf() function.
  } catch(CommunicationException e) {
    std::string msg;
    msg = e.toString();
    GetListener()->OnErrorOccurred(GetRequestID(), e.getResultCode(), msg);
  }
  fprintf(stderr, "ReadFileCommand::Execute End\n");
  delete this;
}


void ReadFileCommand::SeekAtOffsetOf(LIBSSH2_SFTP_HANDLE *sftp_handle,
                                     const libssh2_uint64_t offset)
{
  fprintf(stderr, "ReadFileCommand::SeekAtOffsetOf\n");
  libssh2_sftp_seek64(sftp_handle, offset);
  fprintf(stderr, "ReadFileCommand::SeekAtOffsetOf End\n");
}

void ReadFileCommand::ReadFileLengthOf(LIBSSH2_SFTP_HANDLE *sftp_handle,
                                       const libssh2_uint64_t length,
                                       const unsigned int buffer_size)
  throw(CommunicationException)
{
  fprintf(stderr, "ReadFileCommand::ReadFileLengthOf\n");
  int rc = -1;
  libssh2_uint64_t total = 0;
  const int max_sftp_size = buffer_size * 1024;
  // SFTP read size is unpredictable, so we have to handle up to 2x chunk size.
  char result_buf[max_sftp_size * 2];
  unsigned int buf_offset = 0;
  do {
    int buf_size = std::min((libssh2_uint64_t)max_sftp_size, length - total);
    char* sftp_buffer = result_buf + buf_offset;
    rc = libssh2_sftp_read(sftp_handle, sftp_buffer, buf_size);
    if (rc == LIBSSH2_ERROR_EAGAIN) {
      WaitSocket(GetServerSock(), GetSession());
    } else if (rc >= 0) {
      total += rc;
      buf_offset += rc;
      fprintf(stderr, "SFTP read buf_size: %d, rc:%d total:%llu length:%llu result_buf:%d\n", buf_size, rc, total, length, buf_offset);
      if (rc == 0) {
        fprintf(stderr, "Reading completed - 2\n");
        CloseSftpHandle(sftp_handle);
        OnReadFile(result_buf, buf_offset, false);
        break;
      } else {
        if (length <= total) {
          fprintf(stderr, "Reading completed - 1\n");
          CloseSftpHandle(sftp_handle);
          OnReadFile(result_buf, buf_offset, false);
          break;
        } else if (buf_offset >= (buffer_size * 1024)) {
          fprintf(stderr, "Flush\n");
          OnReadFile(result_buf, buf_offset, true);
          buf_offset = 0;
        }
      }
    } else {
      THROW_COMMUNICATION_EXCEPTION("sftpThreadError_readFileFailed", rc);
    }
  } while (1);
  fprintf(stderr, "ReadFileCommand::ReadFileLengthOf End\n");
}

void ReadFileCommand::OnReadFile(const char *result_buf, int length, bool has_more)
{
  fprintf(stderr, "ReadFileCommand::OnReadFile\n");
  pp::VarArrayBuffer buffer(length);
  char* data = static_cast<char*>(buffer.Map());
  memcpy(data, result_buf, length);
  GetListener()->OnReadFile(GetRequestID(), buffer, length, has_more);
  fprintf(stderr, "ReadFileCommand::OnReadFile End\n");
}
