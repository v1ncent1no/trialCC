#include <cstdio>
#include <ios>
#include <iostream>

#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <string_view>

#include "event_system.h"
#include "parser.h"
#include "timeutil.h"

#if defined(__gnu_linux__) || defined(_SYSTYPE_BSD)
#include <sysexits.h>
#else
#define EX_OK 0 /* successful termination */

#define EX_USAGE 64 /* command line usage error */
#define EX_IOERR 74 /* input/output error */
#endif

// simple map on string literals versions of the errors to be used in programs`
// output
const char *computer_clib_error_str[] = {
    "YouShallNotPass",
    "NotOpenYet",
    "PlaceIsBusy",
    "ClientUnknown",
    "ICanWaitNoLonger!",
};

auto read_file(std::string &str, const char *path) -> bool
{
    std::ifstream t(path);
    if (t.fail())
        return false;

    t.seekg(0, std::ios::end);
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    str.assign(
        (std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

    return true;
}

auto main(int argc, char **argv) -> int
{
    if (argc < 2) {
        fprintf(stderr,
            "USAGE ERROR: no file path supplied\n"
            "USAGE:\n\t%s <path_to_file>\n",
            argv[0]);

        return EX_USAGE;
    }

    std::string source;
    if (!read_file(source, argv[1])) {
        fprintf(stderr, "ERROR: cannot open file %s\n", argv[1]);

        return EX_IOERR;
    }

    BasicParser parser(source);

    event_system::Config cfg {};
    cfg.from_parser(parser);

    event_system::EventSystem system {
        cfg.tables_count,
        cfg.work_hours,
        cfg.hour_cost,
    };

    std::size_t h, m;
    timeutil::time_point_hm(cfg.work_hours.begin, h, m);

    printf("%02zu:%02zu\n", h, m);

    for (event_system::Event e {};
         parser.skip('\n') && e.from_parser(parser);) {

        timeutil::time_point_hm(e.time, h, m);

        printf("%02zu:%02zu %u %.*s ", h, m, e.type,
            (int)e.client_name.length(), e.client_name.data());
        if (e.table_id.has_value())
            printf("%llu", e.table_id.value());
        printf("\n");

        std::optional<event_system::Event> out_e;
        system.handle_event(e, out_e);

        if (!out_e.has_value())
            continue;

        e = out_e.value();

        timeutil::time_point_hm(e.time, h, m);

        if (e.type == event_system::out_error)
            printf("%02zu:%02zu %u %s\n", h, m, e.type,
                computer_clib_error_str[e.error_code.value()]);
        else {
            printf("%02zu:%02zu %u %.*s ", h, m, e.type,
                (int)e.client_name.length(), e.client_name.data());

            if (e.table_id.has_value())
                printf("%llu", e.table_id.value());
            printf("\n");
        }
    }

    std::set<event_system::Event> last_events;
    system.kick_everyone_out(last_events);

    timeutil::time_point_hm(cfg.work_hours.end, h, m);

    for (const auto &e : last_events)
        printf("%02zu:%02zu %u %.*s\n", h, m, e.type,
            (int)e.client_name.length(), e.client_name.data());

    printf("%02zu:%02zu\n", h, m);

    system.write_tables_stats(stdout);

    return EX_OK;
}
