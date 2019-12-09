#include "test.h"
#include "election.h"
#include "heartbeater.h"
#include "sdfs_client.h"
#include "mock_tcp.h"
#include "environment.h"
#include "configuration.h"

using std::unique_ptr;
using std::make_unique;

/*
testing::register_test sdfs_client_put_test(
    "sdfs_client.normal_put",
    "Tests put operation under normal test conditions",
    35, [] (logger::log_level level))
{

}
*/
