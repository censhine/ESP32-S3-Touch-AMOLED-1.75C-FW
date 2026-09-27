idf_component_register(
    SRCS "round_wing_app.cpp" "../../src/game.cpp" "../../src/game_view.cpp"
         "../../assets/roundwing_font_22.c" "../../assets/roundwing_font_30.c"
    INCLUDE_DIRS "." "../../include" "../../assets"
    REQUIRES brookesia_core lvgl nvs_flash
    WHOLE_ARCHIVE
)
