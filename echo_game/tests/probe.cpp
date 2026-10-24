#include "EchoApp.h"
#include "EchoEngine.h"
#include "EchoLevels.h"
#include "TestWorld.h"
#include "ActionButton.h"

#include <assert.h>
#include <cmath>
#include <stdio.h>
#include <string.h>
#include <vector>

using namespace echo_game;

void hotpath_checks();

void ui_checks() {
  hotpath_checks();
  EchoApp app;
  app.initialize(0, nullptr, true);
  assert(app.screen() == Screen::Help);
  assert(app.press_step() == Action::None && app.run().moves() == 0);
  app.tap(180, 288, 0);
  assert(app.screen() == Screen::Playing && app.take_save_request());
  assert(app.tap(180, 180, 0) == Action::None);
  assert(app.tap(202, 124, 100) == Action::Called);
  assert(app.run().calls() == 1 && app.animating(500));
  assert(app.press_step() == Action::Changed);
  assert(app.run().moves() == 1 && app.run().calls() == 1 && !app.animating(700));
  // The bottom of the old button row is a direction tap, never a touch step.
  assert(app.tap(130, 310, 710) == Action::Called);
  assert(app.run().moves() == 1 && app.run().calls() == 2);
  app.tap(180, 30, 800);
  assert(app.screen() == Screen::Menu);
  assert(app.press_step() == Action::None && app.run().moves() == 1);
  assert(app.set_battery(3900, 65));
  assert(!app.set_battery(3900, 65));
  app.tap(180, 184, 800);
  assert(app.screen() == Screen::Rooms);
  app.tap(180, 240, 800);
  assert(app.room_index() == 2 && app.run().calls() == 0 && app.take_save_request());
  app.tap(79, 143, 1000);
  for (int i = 0; i < 8; ++i) app.press_step();
  assert(app.screen() == Screen::Result && app.run().score() == 1000);
  assert(app.best_score(2) == 1000 && app.take_save_request());
  assert(app.press_step() == Action::None && app.run().moves() == 8);
  app.tap(130, 310, 2300);
  assert(app.screen() == Screen::Playing && app.run().moves() == 0);
  app.tap(180, 30, 2400);
  app.tap(180, 272, 2400);
  assert(app.screen() == Screen::Result && app.run().score() == 0);
  assert(app.best_score(2) == 1000);
  app.tap(230, 310, 2500);
  assert(app.screen() == Screen::Playing && app.room_index() == 3);

  // Every room is reachable through its page; selections keep existing scores.
  uint32_t stored[kRoomCount] = {};
  stored[0] = 900; stored[1] = 820; stored[2] = 1000; stored[kRoomCount - 1] = 650;
  for (size_t room = 0; room < kRoomCount; ++room) {
    app.initialize(room, stored, false);
    app.tap(180, 30, 0);
    app.tap(180, 184, 0);
    assert(app.screen() == Screen::Rooms);
    app.tap(180, 107 + static_cast<int>(room % kRoomsPerPage) * 60, 0);
    assert(app.screen() == Screen::Playing && app.room_index() == room);
    assert(app.best_score(0) == 900 && app.best_score(2) == 1000);
  }
  app.initialize(0, stored, false);
  app.tap(180, 30, 0); app.tap(180, 184, 0);
  assert(app.tap(96, 292, 0) == Action::None);  // Disabled first-page PREV.
  for (size_t page = 1; page < kRoomPages; ++page) assert(app.tap(264, 292, 0) == Action::Changed);
  assert(app.tap(264, 292, 0) == Action::None);  // Disabled last-page NEXT.
  assert(app.tap(180, 170, 0) == Action::None);  // Empty last-page card.
  app.tap(180, 107, 0);
  assert(app.room_index() == kRoomCount - 1 && app.best_score(kRoomCount - 1) == 650);
  app.tap(180, 30, 0); app.tap(180, 272, 0);  // Reveal the last room.
  assert(app.screen() == Screen::Result && app.run().score() == 0);
  app.tap(230, 310, 0);
  assert(app.screen() == Screen::Rooms);
  app.tap(180, 292, 0);
  assert(app.screen() == Screen::Result);
  app.tap(130, 310, 0);
  app.tap(180, 30, 0); app.tap(180, 184, 0);
  for (size_t page = 1; page < kRoomPages; ++page) app.tap(96, 292, 0);
  app.tap(180, 107, 0);
  assert(app.room_index() == 0);

  // Partial animation redraws must produce the same frame as a full redraw,
  // including the last clearing frame, while retaining the header and footer.
  std::vector<uint16_t> full(360 * 360), partial(360 * 360);
  for (size_t room = 0; room < kRoomCount; ++room) {
    app.initialize(room, stored, false);
    app.tap(225, 125, 0);
    app.draw(partial.data(), 80);
    for (uint64_t age : {300, 700, 1200, 2500, 5000}) {
      app.draw(partial.data(), age, true);
      app.draw(full.data(), age);
      assert(partial == full);
    }
  }

  // A noisy physical press produces one step and holding never repeats it.
  ActionButton key;
  key.initialize(false, 0);
  assert(!key.update(true, 100));
  assert(!key.update(false, 110));
  assert(!key.update(true, 115));
  assert(!key.update(true, 139));
  assert(key.update(true, 140));
  assert(!key.update(true, 4000));
  assert(!key.update(false, 5000));
  assert(!key.update(true, 5010));
  assert(!key.update(false, 5020));
  assert(!key.update(false, 5045));
  assert(!key.update(true, 5100));
  assert(key.update(true, 5125));
  key.initialize(true, 6000);
  assert(!key.update(true, 7000));
  assert(!key.update(false, 8000));
  assert(!key.update(false, 8025));
  assert(!key.update(true, 8100));
  assert(key.update(true, 8125));
  key.initialize(false, UINT64_C(1) << 33);
  assert(!key.update(true, (UINT64_C(1) << 33) + 10));
  assert(key.update(true, (UINT64_C(1) << 33) + 35));

  Run bounded;
  bounded.load(kLevels[0]);
  for (int i = 0; i < 1200; ++i) bounded.step();
  assert(bounded.path_count() == kPathCapacity && bounded.moves() == 1200);
  assert(std::abs(bounded.path_at(kPathCapacity - 1).y - bounded.position().y) < 1e-8);
  puts("UI and bounded-history checks passed.");
}

void ppm(const char* directory, const char* name, EchoApp& app, uint64_t time) {
  // Sentinel pixels verify that drawing stays inside its framebuffer.
  std::vector<uint16_t> pixels(360 * 360 + 2, 0xA55A);
  app.draw(pixels.data() + 1, time);
  assert(pixels.front() == 0xA55A && pixels.back() == 0xA55A);
  char filename[1024];
  snprintf(filename, sizeof(filename), "%s/%s.ppm", directory, name);
  FILE* file = fopen(filename, "wb");
  assert(file);
  fprintf(file, "P6\n360 360\n255\n");
  for (size_t i = 1; i <= 360 * 360; ++i) {
    const uint16_t p = pixels[i];
    const unsigned char rgb[] = {
      static_cast<unsigned char>(((p >> 11) & 31) * 255 / 31),
      static_cast<unsigned char>(((p >> 5) & 63) * 255 / 63),
      static_cast<unsigned char>((p & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, file);
  }
  fclose(file);
}

int main(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "ui") == 0) { ui_checks(); return 0; }
  if (argc > 2 && strcmp(argv[1], "preview") == 0) {
    EchoApp app;
    app.initialize(0, nullptr, true);
    ppm(argv[2], "intro", app, 0);
    app.tap(180, 288, 0);
    ppm(argv[2], "idle", app, 0);
    app.tap(180, 130, 1000);
    ppm(argv[2], "ping", app, 2100);
    app.tap(180, 30, 2200);
    app.set_battery(3900, 65);
    ppm(argv[2], "menu", app, 2200);
    app.tap(180, 184, 2200);
    ppm(argv[2], "rooms", app, 2200);
    app.tap(180, 110, 2300);
    app.tap(198, 107, 2400);
    for (int i = 0; i < 8; ++i) app.press_step();
    assert(app.run().status() == Status::Escaped);
    ppm(argv[2], "escaped", app, 2600);
    app.initialize(12, nullptr, false);
    app.tap(180, 30, 0); app.tap(180, 184, 0);
    ppm(argv[2], "rooms-middle", app, 0);
    app.initialize(kRoomCount - 1, nullptr, false);
    ppm(argv[2], "final-room", app, 0);
    app.tap(180, 130, 0);
    ppm(argv[2], "final-echo", app, 700);
    app.tap(180, 30, 0); app.tap(180, 184, 0);
    ppm(argv[2], "rooms-last", app, 0);
    app.tap(180, 107, 0);
    app.tap(180, 30, 0); app.tap(180, 272, 0);
    ppm(argv[2], "final-map", app, 0);
    return 0;
  }
  char command[16];
  Run run;
  EchoBuffer echoes;
  while (scanf("%15s", command) == 1) {
    if (strcmp(command, "echo") == 0) {
      int room; double x, y, heading;
      assert(scanf("%d %lf %lf %lf", &room, &x, &y, &heading) == 4);
      emit(room < 0 ? kTestLevel : kLevels[room], {x, y}, heading, echoes);
      printf("%zu\n", echoes.count);
      for (size_t i = 0; i < echoes.count; ++i) {
        const Echo& e = echoes.values[i];
        printf("%.17g %.17g %.17g %.17g %.17g %u %u\n", e.point.x, e.point.y,
               e.distance, e.intensity, e.bearing, static_cast<unsigned>(e.material), e.bounces);
      }
    } else if (strcmp(command, "load") == 0) {
      int room; assert(scanf("%d", &room) == 1); run.load(kLevels[room]);
    } else {
      if (strcmp(command, "call") == 0 || strcmp(command, "turn") == 0) {
        double heading; assert(scanf("%lf", &heading) == 1);
        if (command[0] == 'c') run.call(heading); else run.turn(heading);
      } else if (strcmp(command, "step") == 0) run.step();
      else if (strcmp(command, "reveal") == 0) run.surrender();
      else if (strcmp(command, "reset") == 0) run.restart();
      else return 2;
      printf("%.17g %.17g %.17g %lu %lu %d %u %lu\n", run.position().x, run.position().y,
             run.heading(), static_cast<unsigned long>(run.calls()), static_cast<unsigned long>(run.moves()),
             run.blocked(), static_cast<unsigned>(run.status()), static_cast<unsigned long>(run.score()));
    }
  }
}

