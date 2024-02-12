#include <algorithm>
#include <cmath>

#include "event_system.h"

namespace event_system {

Table::Table(std::size_t id)
    : id(id)
{
}

[[nodiscard]] constexpr auto Table::occupied() const -> bool
{
    return is_occupied;
}

auto Table::sit(timeutil::TimePoint time) -> void
{
    last_sit    = time;
    is_occupied = true;
}

auto Table::leave(timeutil::TimePoint time, std::size_t hour_cost) -> void
{
    if (!is_occupied) {
        return; // no-op, as it was not sitted on at the time
    }

    auto delta = time - last_sit.value();
    last_sit   = std::nullopt;

    total_mins += delta;
    revenue += static_cast<size_t>(std::ceil(static_cast<double>(delta) / 60.0))
        * hour_cost;

    is_occupied = false;
}

auto Table::write_stats_to_file(FILE *f) const -> int
{
    std::size_t h, m;
    timeutil::time_point_hm(total_mins, h, m);

    fprintf(f, "%lu %lu %02lu:%02lu", id, revenue, h, m);

    return ferror(f);
}

[[nodiscard]] constexpr auto Table::get_id() const -> std::size_t { return id; }

auto Event::operator<(const Event &other) const -> bool
{
    return client_name[0] < other.client_name[0];
}

// FORMAT: [TIME POINT] [SPACE] [EVENT_ID] [SPACE] [CLIENT] [SPACE] \
    // ([TABLE_ID])
auto Event::from_parser(BasicParser &parser) -> bool
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

// FORMAT: [TABLES COUNT] [NEW LINE]
//         [TIME INTERVAL] [NEW LINE]
//         [HOUR COST]
auto Config::from_parser(BasicParser &parser) -> bool
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

EventSystem::EventSystem(std::size_t tables_count,
    timeutil::TimeInterval work_hours, std::size_t hour_cost)
    : work_hours(work_hours)
    , hour_cost(hour_cost)
    , clients(tables_count * 2)
{
    for (std::size_t i = 1; i <= tables_count; ++i)
        tables.emplace_back(i);
}

auto EventSystem::is_queue_full() const -> bool
{
    return std::count_if(clients.cbegin(), clients.cend(),
               [](const auto &pair) {
                   return pair.second.state == client_state_awaits;
               })
        >= tables.size();
}

auto EventSystem::sit_client_table(std::string_view client_name, std::size_t id,
    timeutil::TimePoint time) -> void
{
    auto &client = clients[client_name];

    client.table_id = id;
    client.state    = client_state_sits;
    tables[id - 1].sit(time);
}

auto EventSystem::handle_client_came_in(
    const Event &event, std::optional<Event> &out_event) -> void
{
    if (!work_hours.contains(event.time)) {
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

auto EventSystem::handle_client_sit(
    const Event &event, std::optional<Event> &out_event) -> void
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

auto EventSystem::handle_client_awaiting(
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

auto EventSystem::handle_client_left(
    const Event &event, std::optional<Event> &out_event) -> void
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

auto EventSystem::handle_unexpected(std::optional<Event> &out_event) -> void
{
    abort(); // FIXME
}

auto EventSystem::handle_event(Event &event, std::optional<Event> &out_event)
    -> void
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

auto EventSystem::kick_everyone_out(std::set<Event> &events) -> void
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

auto EventSystem::write_tables_stats(FILE *f) -> int
{
    bool err = false;

    for (const Table &t : tables) {
        err |= t.write_stats_to_file(f);
        err |= putc('\n', f);
    }

    return err;
}

} // namespace event_system
