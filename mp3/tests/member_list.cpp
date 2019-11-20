#include "test.h"
#include "member_list.h"
#include "environment.h"

#include <vector>
#include <algorithm>
#include <string>
#include <memory>

bool compare_hostnames(const member &m1, const member &m2) {
    return (m1.hostname.compare(m2.hostname) < 0) ? true : false;
}

testing::register_test inserting_small("member_list.inserting_small",
    "Tests inserting / reading member lists with less than 5 members", [] (logger::log_level level)
{
    environment env(true);
    env.get<configuration>()->set_hostname("local");
    env.get<logger_factory>()->configure(level);

    member_list ml(env);
    std::vector<int> ids;
    ids.push_back(ml.add_member("1", std::hash<std::string>()("1")));
    ids.push_back(ml.add_member("2", std::hash<std::string>()("2")));
    ids.push_back(ml.add_member("local", std::hash<std::string>()("local")));

    // Check that all the generated IDs are unique
    assert(std::unique(ids.begin(), ids.end()) == ids.end());

    std::vector<member> neighbors = ml.get_neighbors();
    assert(neighbors.size() == 2);

    std::sort(neighbors.begin(), neighbors.end(), compare_hostnames);
    assert(neighbors[0].hostname == "1");
    assert(neighbors[1].hostname == "2");
});

testing::register_test inserting_large("member_list.inserting_large",
    "Tests inserting / reading / sortedness of member list with more than 5 members", [] (logger::log_level level)
{
    environment env(true);
    env.get<configuration>()->set_hostname("local");
    env.get<logger_factory>()->configure(level);

    member_list ml(env);
    std::vector<int> ids;
    for (int i = 1; i < 11; i++) {
        ids.push_back(ml.add_member(std::to_string(i), std::hash<int>()(i)));
    }
    ids.push_back(ml.add_member("local", std::hash<std::string>()("local")));

    // Check that all the generated IDs are unique
    assert(std::unique(ids.begin(), ids.end()) == ids.end());

    std::vector<member> neighbors = ml.get_neighbors();
    assert(neighbors.size() == 4);

    assert(std::unique(neighbors.begin(), neighbors.end()) == neighbors.end());
    assert(std::find_if(neighbors.begin(), neighbors.end(), [](member m) {return m.hostname == "local";}) == neighbors.end());

    // Check that the list of members is correct
    std::vector<member> list = ml.get_members();
    assert(list.size() == 11);

    // Check for sortedness
    uint32_t prev_id = 0;
    for (member m : list) {
        assert(m.id > prev_id);
        prev_id = m.id;
    }
});

testing::register_test removing("member_list.removing_large",
    "Tests removing from member list, while verifying sortedness and neighbors", [] (logger::log_level level)
{
    environment env(true);
    env.get<configuration>()->set_hostname("8");
    env.get<logger_factory>()->configure(level);

    member_list ml(env);
    std::vector<int> ids;
    for (int i = 1; i < 8; i++) {
        ids.push_back(ml.add_member(std::to_string(i), std::hash<int>()(i)));
    }
    ids.push_back(ml.add_member("8", std::hash<int>()(8)));
    // ml: 1, 2, 3, 4, 5, 6, 7, 8

    for (int i = 0; i < 8; i += 2) {
        ml.remove_member(ids[i]);
    }
    // ml: 2, 4, 6, 8

    std::vector<member> list = ml.get_members();
    assert(list.size() == 4);

    // Check for sortedness
    uint32_t prev_id = 0;
    for (member m : list) {
        assert(m.id > prev_id);
        prev_id = m.id;
    }
    // Check that all the members that should be there are there
    std::sort(list.begin(), list.end(), compare_hostnames);
    int i = 2;
    for (auto it = list.begin(); it != list.end(); it++) {
        assert(it->hostname == std::to_string(i));
        i += 2;
    }

    // Check that the neighbors are correct
    std::vector<member> neighbors = ml.get_neighbors();
    assert(neighbors.size() == 3);
});
