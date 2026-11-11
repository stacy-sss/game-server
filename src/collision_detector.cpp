#include "collision_detector.h"
#include <cassert>


namespace collision_detector {

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    // Проверим, что перемещение ненулевое.
    // Тут приходится использовать строгое равенство, а не приближённое,
    // пскольку при сборе заказов придётся учитывать перемещение даже на небольшое
    // расстояние.
    assert(b.x != a.x || b.y != a.y);
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

// В задании на разработку тестов реализовывать следующую функцию не нужно -
// она будет линковаться извне.

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> events;
    for (size_t g = 0; g < provider.GatherersCount(); g++) {
        Gatherer gth = provider.GetGatherer(g);
        double v_x = gth.end_pos.x - gth.start_pos.x;
        double v_y = gth.end_pos.y - gth.start_pos.y;
        double v_len2 = v_x* v_x + v_y * v_y;

        if (v_len2 == 0.0) {
            continue;
        }
        for (size_t i = 0; i < provider.ItemsCount(); i++) {
            Item itm = provider.GetItem(i);
            double u_x = itm.position.x - gth.start_pos.x;
            double u_y = itm.position.y - gth.start_pos.y;
            double u_dot_v = u_x * v_x + u_y * v_y;
            double u_len2 = u_x * u_x + u_y * u_y;
            double proj_ratio = u_dot_v / v_len2;
            double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

            double collect_rad = (itm.width + gth.width) / 2.0;
            if (proj_ratio >= 0.0 && proj_ratio <= 1.0 &&
                sq_distance <= collect_rad * collect_rad) {
                events.push_back(GatheringEvent{
                .item_id = i,
                .gatherer_id = g,
                .sq_distance = sq_distance,
                .time = proj_ratio
                });
            }
        }
    }
    std::sort(events.begin(), events.end(), [](const GatheringEvent& a, const GatheringEvent& b) {
        return a.time < b.time;
    });
    return events;
}


}  // namespace collision_detector
