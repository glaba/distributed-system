#include "test.h"
#include "environment.h"
#include "configuration.h"
#include "sdfs_client.h"

#include <stdlib.h>
#include <cassert>

using std::string;

void setup_env(environment &env) {
    configuration *config = env.get<configuration>();
    config->set_dir("./dir/");
    config->set_sdfs_subdir("mock_sdfs");

    sdfs_client *sdfsc = env.get<sdfs_client>();
    sdfsc->start();
}

void create_file(environment &env, string filename, string contents) {
    // Create a file with some random data
    FILE *stream = popen(("echo -n \"" + contents + "\" > " + env.get<configuration>()->get_dir() + filename).c_str(), "r");
    assert(stream);
    pclose(stream);
}

int put_file_sdfs(environment &env, string filename, string sdfs_filename) {
    return env.get<sdfs_client>()->put_operation(env.get<configuration>()->get_dir() + filename, sdfs_filename);
}

int append_file_sdfs(environment &env, string filename, string sdfs_filename) {
    return env.get<sdfs_client>()->append_operation(env.get<configuration>()->get_dir() + filename, sdfs_filename);
}

int get_file_sdfs(environment &env, string filename, string sdfs_filename) {
    return env.get<sdfs_client>()->get_operation(env.get<configuration>()->get_dir() + filename, sdfs_filename);
}

void diff_files(environment &env, string first, string second) {
    string dir = env.get<configuration>()->get_dir();
    FILE *stream = popen(("diff " + dir + first + " " + dir + second).c_str(), "r");
    assert(stream);
    char buffer[1024];
    int bytes_read = fread(static_cast<void*>(buffer), 1, 1024, stream);
    assert(bytes_read == 0 && feof(stream));
    pclose(stream);
}

void delete_file(environment &env, string filename) {
    string dir = env.get<configuration>()->get_dir();
    FILE *stream = popen(("rm " + dir + filename).c_str(), "r"); pclose(stream);
    assert(stream);
}

int delete_file_sdfs(environment &env, string sdfs_filename) {
    return env.get<sdfs_client>()->del_operation(sdfs_filename);
}

testing::register_test put_get("mock_sdfs_client.put_get",
    "Tests putting and subsequently getting a file from the SDFS",
    1, [] (logger::log_level level)
{
    environment env(true);
    setup_env(env);
    create_file(env, "original", "data");
    assert(put_file_sdfs(env, "original", "put_get") == 0);
    assert(get_file_sdfs(env, "copy", "put_get") == 0);
    diff_files(env, "original", "copy");
    delete_file(env, "original");
    delete_file(env, "copy");
});

testing::register_test delete_file_test("mock_sdfs_client.delete",
    "Tests putting, deleting and attempting to get a file from the SDFS",
    1, [] (logger::log_level level)
{
    environment env(true);
    setup_env(env);
    create_file(env, "original", "data");
    assert(put_file_sdfs(env, "original", "delete") == 0);
    assert(delete_file_sdfs(env, "delete") == 0);
    assert(get_file_sdfs(env, "copy", "delete") != 0);
    delete_file(env, "original");
});

testing::register_test append_file_test("mock_sdfs_client.append",
    "Tests appending to a file in the SDFS",
    1, [] (logger::log_level level)
{
    environment env(true);
    setup_env(env);
    for (unsigned i = 0; i < 10; i++) {
        create_file(env, "original" + std::to_string(i), std::to_string(i));
        assert(append_file_sdfs(env, "original" + std::to_string(i), "combined") == 0);
        delete_file(env, "original" + std::to_string(i));
    }
    assert(get_file_sdfs(env, "copy", "combined") == 0);
    create_file(env, "correct", "0123456789");
    diff_files(env, "correct", "copy");
    delete_file(env, "correct");
    delete_file(env, "copy");
});

testing::register_test shared_put_get("mock_sdfs_client.shared_put_get",
    "Tests putting and subsequently getting a file from the SDFS from two different nodes",
    1, [] (logger::log_level level)
{
    environment_group env_group(true);

    std::unique_ptr<environment> env1 = env_group.get_env();
    std::unique_ptr<environment> env2 = env_group.get_env();

    setup_env(*env1);
    setup_env(*env2);
    create_file(*env1, "original", "data");
    create_file(*env2, "original", "data");
    assert(put_file_sdfs(*env1, "original", "put_get") == 0);

    assert(get_file_sdfs(*env2, "copy", "put_get") == 0);
    diff_files(*env2, "original", "copy");
    delete_file(*env1, "original");
    delete_file(*env2, "original");
    delete_file(*env2, "copy");
});

testing::register_test shared_delete_file_test("mock_sdfs_client.shared_delete",
    "Tests putting, deleting and attempting to get a file from the SDFS from two different nodes",
    1, [] (logger::log_level level)
{
    environment_group env_group(true);

    std::unique_ptr<environment> env1 = env_group.get_env();
    std::unique_ptr<environment> env2 = env_group.get_env();

    setup_env(*env1);
    setup_env(*env2);
    create_file(*env1, "original", "data");
    assert(put_file_sdfs(*env1, "original", "delete") == 0);
    assert(delete_file_sdfs(*env2, "delete") == 0);
    assert(get_file_sdfs(*env2, "copy", "delete") != 0);
    delete_file(*env1, "original");
});

testing::register_test shared_append_file_test("mock_sdfs_client.shared_append",
    "Tests appending to a file in the SDFS from two different nodes",
    1, [] (logger::log_level level)
{
    environment_group env_group(true);

    std::unique_ptr<environment> env1 = env_group.get_env();
    std::unique_ptr<environment> env2 = env_group.get_env();

    setup_env(*env1);
    setup_env(*env2);
    for (unsigned i = 0; i < 10; i++) {
        environment &env_to_use = (i % 2 == 0) ? *env1 : *env2;
        create_file(env_to_use, "original" + std::to_string(i), std::to_string(i));
        assert(append_file_sdfs(env_to_use, "original" + std::to_string(i), "combined") == 0);
        delete_file(env_to_use, "original" + std::to_string(i));
    }
    assert(get_file_sdfs(*env2, "copy", "combined") == 0);
    create_file(*env2, "correct", "0123456789");
    diff_files(*env2, "correct", "copy");
    delete_file(*env2, "correct");
    delete_file(*env2, "copy");
});
