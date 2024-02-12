#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ios>
#include <iostream>

#include <cstddef>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

#if defined(__gnu_linux__) || defined(_SYSTYPE_BSD)
#include <sysexits.h>
#else
#define EX_OK 0 /* successful termination */

#define EX_USAGE     64 /* command line usage error */
#define EX_DATAERR   65 /* data format error */
#define EX_NOINPUT   66 /* cannot open input */
#define EX_CANTCREAT 73 /* can't create (user) output file */
#define EX_IOERR     74 /* input/output error */
#define EX_NOPERM    77 /* permission denied */
#endif

// ████████╗██╗███╗   ███╗███████╗██╗   ██╗████████╗██╗██╗
// ╚══██╔══╝██║████╗ ████║██╔════╝██║   ██║╚══██╔══╝██║██║
//    ██║   ██║██╔████╔██║█████╗  ██║   ██║   ██║   ██║██║
//    ██║   ██║██║╚██╔╝██║██╔══╝  ██║   ██║   ██║   ██║██║
//    ██║   ██║██║ ╚═╝ ██║███████╗╚██████╔╝   ██║   ██║███████╗
//    ╚═╝   ╚═╝╚═╝     ╚═╝╚══════╝ ╚═════╝    ╚═╝   ╚═╝╚══════╝
namespace timeutil {

using TimePoint = std::size_t;

auto make_time_point(std::size_t hours, std::size_t minutes) -> TimePoint
{
    return hours * 60 + minutes;
}

auto time_point_hm(const TimePoint &p, std::size_t &hours, std::size_t &minutes)
    -> void
{
    hours   = p / 60;
    minutes = p % 60;
}

struct TimeInterval {
    TimePoint begin;
    TimePoint end;

    auto conatains(TimePoint p) -> bool { return p >= begin && p <= end; }
};

} // namespace timeutil

// ██████╗  █████╗ ██████╗ ███████╗███████╗██╗   ██╗████████╗██╗██╗
// ██╔══██╗██╔══██╗██╔══██╗██╔════╝██╔════╝██║   ██║╚══██╔══╝██║██║
// ██████╔╝███████║██████╔╝███████╗█████╗  ██║   ██║   ██║   ██║██║
// ██╔═══╝ ██╔══██║██╔══██╗╚════██║██╔══╝  ██║   ██║   ██║   ██║██║
// ██║     ██║  ██║██║  ██║███████║███████╗╚██████╔╝   ██║   ██║███████╗
// ╚═╝     ╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚══════╝ ╚═════╝    ╚═╝   ╚═╝╚══════╝
class BasicParser {
    std::string                 source;
    std::string::const_iterator pointer;

    // reson to create local is_digit, is because the cctype one does also
    // include characters 'a'..'z' for hex nums
    static inline auto is_digit(const char c) -> bool
    {
        return c >= '0' && c <= '9';
    }

    static inline auto is_word(const char c) -> bool
    {
        return (c >= 'a' && c <= 'z') || is_digit(c) || c == '_' || c == '-';
    }

public:
    BasicParser(std::string source)
        : source(std::move(source))
        , pointer(this->source.cbegin())
    {
    }

    // needed to check for '\n' and ' ' between tokens
    auto skip(const char expected) -> bool
    {
        bool result = *pointer == expected;
        ++pointer;

        return result;
    }

    auto number() -> std::optional<std::size_t>
    {
        std::optional<std::size_t> result {};

        std::size_t n { 0 };
        while (pointer != source.cend() && is_digit(*pointer))
            result = n = n * 10 + (*pointer++ - '0');

        return result;
    }

    // FORMAT: HH [COLON] MM
    auto time_point() -> std::optional<timeutil::TimePoint>
    {
        const auto hours = number();
        if (!hours.has_value()) {
            return std::nullopt;
        }

        if (!skip(':')) {
            return std::nullopt;
        }

        const auto minutes = number();
        if (!minutes.has_value()) {
            return std::nullopt;
        }

        return timeutil::make_time_point(hours.value(), minutes.value());
    }

    // FORMAT: TIME_POINT [SPACE] TIME_POINT
    auto time_interval() -> std::optional<timeutil::TimeInterval>
    {
        const auto begin = time_point();
        if (!begin.has_value()) {
            return std::nullopt;
        }

        if (!skip(' ')) {
            return std::nullopt;
        }

        const auto end = time_point();
        if (!end.has_value()) {
            return std::nullopt;
        }

        return timeutil::TimeInterval { begin.value(), end.value() };
    }

    auto word() -> std::optional<std::string_view>
    {
        const auto begin = pointer;

        while (pointer != source.cend() && is_word(*pointer))
            pointer++;

        if (begin == pointer)
            return std::nullopt;

        // TODO: check if the end pointer correct for this case
        return std::string_view { begin, pointer };
    }
};

enum ComputerClubError {
    err_client_already_in,  // - caused when `in_client_came_in` event is called
                            // on the user, who has not leave yet
    err_client_came_early,  // - caused when `in_client_came_in` event is called
                            // in the non-working hours
    err_client_table_taken, // - caused when `in_client_sit` is called, but a
                            // table is already being used by other client
    err_client_unknown, // - caused when `in_client_sit` or `in_client_left` are
                        // called on wrong client
    err_client_awaits_nothing, // - caused when `in_client_awaiting` is called,
                               // but there are some at least one table is
                               // available
};

// simple map on string literals versions of the errors to be used in programs`
// output
const char *computer_clib_error_str[] = {
    "YouShallNotPass",
    "NotOpenYet",
    "PlaceIsBusy",
    "ClientUnknown",
    "ICanWaitNoLonger!",
};

// ███████╗██╗   ██╗███████╗███╗   ██╗████████╗███████╗
// ██╔════╝██║   ██║██╔════╝████╗  ██║╚══██╔══╝██╔════╝
// █████╗  ██║   ██║█████╗  ██╔██╗ ██║   ██║   ███████╗
// ██╔══╝  ╚██╗ ██╔╝██╔══╝  ██║╚██╗██║   ██║   ╚════██║
// ███████╗ ╚████╔╝ ███████╗██║ ╚████║   ██║   ███████║
// ╚══════╝  ╚═══╝  ╚══════╝╚═╝  ╚═══╝   ╚═╝   ╚══════╝
namespace event_system {

enum EventType {
    // events read by the program from the file:
    in_client_came_in  = 1,
    in_client_sit      = 2,
    in_client_awaiting = 3,
    in_client_left     = 4,
    // events generated by the program:
    out_client_left    = 11,
    out_client_sit     = 12,
    out_error          = 13,
};

class Table {
    std::size_t id; // should never be equal to 0

    bool is_occupied { false };

    std::size_t revenue { 0 };
    std::size_t total_mins { 0 };

    std::optional<timeutil::TimePoint> last_sit {};

public:
    explicit Table(std::size_t id)
        : id(id)
    {
    }

    [[nodiscard]] constexpr auto occupied() const -> bool
    {
        return is_occupied;
    }

    auto sit(timeutil::TimePoint time) -> void
    {
        last_sit    = time;
        is_occupied = true;
    }

    auto leave(timeutil::TimePoint time, std::size_t hour_cost) -> void
    {
        if (!is_occupied) {
            return; // no-op, as it was not sitted on at the time
        }

        auto delta = time - last_sit.value();
        last_sit   = std::nullopt;

        total_mins += delta;
        revenue
            += static_cast<size_t>(std::ceil(static_cast<double>(delta) / 60.0))
            * hour_cost;

        is_occupied = false;
    }

    auto write_stats_to_file(FILE *f) const -> int
    {
        std::size_t h, m;
        timeutil::time_point_hm(total_mins, h, m);

        fprintf(f, "%lu %lu %02lu:%02lu", id, revenue, h, m);

        return ferror(f);
    }

    [[nodiscard]] constexpr auto get_id() const -> std::size_t { return id; }
};

struct Event {
    timeutil::TimePoint              time;
    // NOTE: Events that have type `out_error` but have no error
    // code are the ones that are not in the trial specification and
    // assumed to be critical errors
    EventType                        type;
    std::string_view                 client_name;
    std::optional<std::size_t>       table_id;
    std::optional<ComputerClubError> error_code;

    // TODO: add some shortcut constructors

    auto operator<(const Event &other) const -> bool
    {
        return client_name[0] < other.client_name[0];
    }

    // FORMAT: [TIME POINT] [SPACE] [EVENT_ID] [SPACE] [CLIENT] [SPACE] \
    // ([TABLE_ID])
    // TODO: move to Parser implementation
    auto from_parser(BasicParser &parser) -> bool
    {
        // [TIME POINT] [SPACE]
        const auto time = parser.time_point();
        if (!time.has_value()) {
            return false;
        }

        if (!parser.skip(' ')) {
            return false;
        }

        // [EVENT_ID] [SPACE]
        const auto type = parser.number();
        if (!type.has_value()) {
            return false;
        }

        if (type < in_client_came_in || type > in_client_left)
            return false;

        if (!parser.skip(' ')) {
            return false;
        }

        // [CLIENT] [SPACE]
        const auto client_name = parser.word();
        if (!client_name.has_value()) {
            return false;
        }

        // ([TABLE_ID])
        std::optional<std::size_t> table_id = std::nullopt;
        if (type.value() == in_client_sit) {
            if (!parser.skip(' ')) {
                return false;
            }

            table_id = parser.number();
            if (!table_id.has_value()) {
                return false;
            }
        }

        this->time        = time.value();
        this->type        = static_cast<EventType>(type.value());
        this->client_name = client_name.value();
        this->table_id    = table_id;

        return true;
    }

    auto write_to_file(FILE *f) const -> bool
    {
        // FIXME: implement
        abort();
    }
};

struct Config {
    std::size_t            tables_count;
    timeutil::TimeInterval work_hours;
    std::size_t            hour_cost;

    // FORMAT: [TABLES COUNT] [NEW LINE]
    //         [TIME INTERVAL] [NEW LINE]
    //         [HOUR COST]
    // TODO: move to Parser implementation
    auto from_parser(BasicParser &parser) -> bool
    {
        // first line: TABLES COUNT
        const auto tables_count = parser.number();
        if (!tables_count.has_value()) {
            return false;
        }

        if (!parser.skip('\n')) {
            return false;
        }

        // second line: WORK HOURS
        const auto work_hours = parser.time_interval();
        if (!work_hours.has_value()) {
            return false;
        }

        if (!parser.skip('\n')) {
            return false;
        }

        // second line: HOUR COST
        const auto hour_cost = parser.number();
        if (!hour_cost.has_value()) {
            return false;
        }

        // NOTE: we do not !skip the next character of '\n' or '\0' for
        // consistency, as it's better to check for that in event stream

        this->tables_count = tables_count.value();
        this->work_hours   = work_hours.value();
        this->hour_cost    = hour_cost.value();

        return true;
    }
};

enum ClientState {
    client_state_inside,
    client_state_awaits,
    client_state_sits,
};

struct Client {
    std::optional<std::size_t> table_id {};
    ClientState                state { client_state_inside };
};

class EventSystem {
    std::unordered_map<std::string_view, Client> clients;
    std::vector<Table>                           tables;
    timeutil::TimeInterval                       work_hours;
    std::size_t                                  hour_cost;

public:
    EventSystem(std::size_t tables_count, timeutil::TimeInterval work_hours,
        std::size_t hour_cost)
        : work_hours(work_hours)
        , hour_cost(hour_cost)
        , clients(tables_count * 2)
    {
        for (std::size_t i = 1; i <= tables_count; ++i)
            tables.emplace_back( i );
    }

private:
    auto is_queue_full() const -> bool
    {
        return std::count_if(clients.cbegin(), clients.cend(),
                   [](const auto &pair) {
                       return pair.second.state == client_state_awaits;
                   })
            >= tables.size();
    }

    auto sit_client_table(std::string_view client_name, std::size_t id,
        timeutil::TimePoint time) -> void
    {
        auto &client = clients[client_name];

        client.table_id = id;
        client.state    = client_state_sits;
        tables[id - 1].sit(time);
    }

    auto handle_client_came_in(
        const Event &event, std::optional<Event> &out_event) -> void
    {
        if (!work_hours.conatains(event.time)) {
            out_event = Event {
                .time        = event.time,
                .type        = out_error,
                .client_name = event.client_name,
                .error_code  = err_client_came_early,
            };

            return;
        }

        if (clients.contains(event.client_name)) {
            out_event = Event {
                .time        = event.time,
                .type        = out_error,
                .client_name = event.client_name,
                .error_code  = err_client_already_in,
            };

            return;
        }

        clients.insert({
            event.client_name,
            Client {},
        });
    }

    auto handle_client_sit(const Event &event, std::optional<Event> &out_event)
        -> void
    {
        if (!clients.contains(event.client_name)) {
            out_event = Event {
                .time        = event.time,
                .type        = out_error,
                .client_name = event.client_name,
                .error_code  = err_client_unknown,
            };
        } else if (!event.table_id.has_value()) {
            // NOTE: when we do not supply error_code, but out_error, this means
            // that event is supplied in not correct format
            out_event = Event {
                .time        = event.time,
                .type        = out_error,
                .client_name = event.client_name,
            };
        } else if (tables[event.table_id.value() - 1].occupied()) {
            out_event = Event {
                .time        = event.time,
                .type        = out_error,
                .client_name = event.client_name,
                .error_code  = err_client_table_taken,
            };
        } else {
            auto client = clients[event.client_name];
            if (client.state == client_state_sits) {
                tables[client.table_id.value() - 1].leave(event.time, hour_cost);
            }

            const auto table_id = event.table_id.value();
            sit_client_table(event.client_name, table_id, event.time);
        }
    }

    auto handle_client_awaiting(
        const Event &event, std::optional<Event> &out_event) -> void
    {
        if (!clients.contains(event.client_name)) {
            out_event = Event {
                .time        = event.time,
                .type        = out_error,
                .client_name = event.client_name,
                .error_code  = err_client_unknown,
            };
        } else if (clients.size() < tables.size()) {
            out_event = Event {
                .time        = event.time,
                .type        = out_error,
                .client_name = event.client_name,
                .error_code  = err_client_awaits_nothing,
            };
        } else if (is_queue_full()) {
            // if the queue is full, then we kick the client out with
            // `out_client_left` (id = 11)
            out_event = Event {
                .time        = event.time,
                .type        = out_client_left,
                .client_name = event.client_name,
            };
        } else {
            clients[event.client_name].state = client_state_awaits;
        }
    }

    auto handle_client_left(const Event &event, std::optional<Event> &out_event)
        -> void
    {
        if (!clients.contains(event.client_name)) {
            out_event = Event {
                .time        = event.time,
                .type        = out_error,
                .client_name = event.client_name,
                .error_code  = err_client_unknown,
            };
        } else {
            if (clients[event.client_name].state == client_state_sits) {
                Table &table
                    = tables[clients[event.client_name].table_id.value() - 1];

                table.leave(event.time, hour_cost);
                clients.erase(event.client_name);

                // find the first client from the queue
                auto client = std::find_if(
                    clients.begin(), clients.end(), [](const auto &pair) {
                        return pair.second.state == client_state_awaits;
                    });

                if (client != clients.end()) {
                    sit_client_table(client->first, table.get_id(), event.time);

                    out_event = Event {
                        .time        = event.time,
                        .type        = out_client_sit,
                        .client_name = client->first,
                    };
                }
            }
        }
    }

    auto handle_unexpected(std::optional<Event> &out_event) -> void
    {
        abort(); // FIXME
    }

public:
    // to make it a bit more efficent, the resulting value will be stored in the
    // same event it got
    auto handle_event(Event &event, std::optional<Event> &out_event) -> void
    {
        switch (event.type) {
        case in_client_came_in: {
            handle_client_came_in(event, out_event);
            break;
        }
        case in_client_sit: {
            handle_client_sit(event, out_event);
            break;
        }
        case in_client_awaiting: {
            handle_client_awaiting(event, out_event);
            break;
        }
        case in_client_left: {
            handle_client_left(event, out_event);
            break;
        }
        default:
            // should never happen, otherwise it's an error
            handle_unexpected(out_event);
            return;
        }
    }

    auto kick_everyone_out(std::set<Event> &events) -> void
    {
        for (const auto &pair : clients) {
            events.insert(Event {
                .time        = work_hours.end,
                .type        = out_client_left,
                .client_name = pair.first,
            });

            if (pair.second.table_id.has_value())
                this->tables[pair.second.table_id.value() - 1].leave(
                    work_hours.end, hour_cost);
        }

        clients.clear();
    }

    auto write_tables_stats(FILE *f) -> int
    {
        bool err = false;

        for (const Table &t : tables) {
            err |= t.write_stats_to_file(f);
            err |= putc('\n', f);
        }

        return err;
    }
};

} // namespace event_system;

auto main(int argc, char **argv) -> int
{
    std::string source { "3\n"
                         "09:00 19:00\n"
                         "10\n"
                         "08:48 1 client1\n"
                         "09:41 1 client1\n"
                         "09:48 1 client2\n"
                         "09:52 3 client1\n"
                         "09:54 2 client1 1\n"
                         "10:25 2 client2 2\n"
                         "10:58 1 client3\n"
                         "10:59 2 client3 3\n"
                         "11:30 1 client4\n"
                         "11:35 2 client4 2\n"
                         "11:45 3 client4\n"
                         "12:33 4 client1\n"
                         "12:43 4 client2\n"
                         "15:52 4 client4\n" };

    BasicParser parser(source);

    event_system::Config cfg {};
    cfg.from_parser(parser);

    printf("tables_count:\t\t%lu\nshift begin:\t\t%lu\nshift "
           "end:\t\t%lu\nhour_cost:\t\t%lu\n\n",
        cfg.tables_count, cfg.work_hours.begin, cfg.work_hours.end,
        cfg.hour_cost);

    event_system::EventSystem system {
        cfg.tables_count,
        cfg.work_hours,
        cfg.hour_cost,
    };

    for (event_system::Event e {};
         parser.skip('\n') && e.from_parser(parser);) {

        std::size_t h, m;
        timeutil::time_point_hm(e.time, h, m);

        printf("[INCOMING EVENT] %02lu:%02lu\t%u\t%.*s\t%lu\n", h, m, e.type,
            (int)e.client_name.length(), e.client_name.data(),
            e.table_id.value_or(0));

        std::optional<event_system::Event> out_e;
        system.handle_event(e, out_e);

        if (!out_e.has_value())
            continue;

        e = out_e.value();

        timeutil::time_point_hm(e.time, h, m);

        if (e.type == event_system::out_error)
            printf("[OUTGOING EVENT] %02lu:%02lu\t%u\t%s\n", h, m, e.type,
                computer_clib_error_str[e.error_code.value()]);
        else
            printf("[OUTGOING EVENT] %02lu:%02lu\t%u\t%.*s\t%lu\n", h, m,
                e.type, (int)e.client_name.length(), e.client_name.data(),
                e.table_id.value_or(0));
    }

    std::set<event_system::Event> last_events;
    system.kick_everyone_out(last_events);

    for (const auto &e : last_events) {
        std::size_t h, m;
        timeutil::time_point_hm(e.time, h, m);

        printf("[OUTGOING EVENT] %02lu:%02lu\t%u\t%.*s\t%lu\n", h, m, e.type,
            (int)e.client_name.length(), e.client_name.data(),
            e.table_id.value_or(0));
    }

    // TODO: print the table's stats
    system.write_tables_stats(stdout);

    return EX_OK;
}
