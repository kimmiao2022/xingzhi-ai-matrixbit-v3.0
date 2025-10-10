
    #include "wifi_board.h"
    #include "audio_codecs/es8311_audio_codec.h"
    #include "display/lcd_display.h"

    #include "system_reset.h"

    #include "application.h"
    #include "button.h"
    #include "config.h"
    #include "i2c_device.h"
    #include "iot/thing_manager.h"

    #include <esp_log.h>
    #include <esp_lcd_panel_vendor.h>
    #include <driver/i2c_master.h>
    #include <driver/spi_common.h>

    #include "power_manager.h"
    #include "power_save_timer.h"

    #include "led/single_led.h"
    #include "assets/lang_config.h"
    #include <driver/rtc_io.h>
    #include <esp_sleep.h>
    #include <wifi_station.h>
    #include "esp32_camera.h"
    #include "rp2040iic.h"
    #include "aht30_sensor.h"
    #include "sc7a20h.h"

    #include "uartcmdsend.h"

    #include "board.h"  // 确保包含Board类的头文件
    #include "esp_vfs_fat.h"
    #include <string.h>
    #include <sys/unistd.h>
    #include <sys/stat.h>
    #include <stdint.h>  // 添加这行，用于 PRIu32 宏   
    #include "esp_private/sdmmc_common.h"

    // 定义缓存大小（可根据内存调整，32KB比较均衡）

    #define TAG "XINGZHI_CUBE_1_54_TFT_MATRIXBIT_ML307"

    #define SD_READ_CACHE_SIZE (32 * 1024)
    // 每个文件句柄对应的缓存结构
    typedef struct {
        FILE* fp;                // 真实文件指针
        uint8_t buffer[SD_READ_CACHE_SIZE];
        size_t len;              // 缓存中有效字节数
        size_t pos;              // 当前读取位置
    } sd_file_cache_t;   

    LV_FONT_DECLARE(font_puhui_20_4);
    LV_FONT_DECLARE(font_awesome_20_4);
    LV_FONT_DECLARE(font_puhui_14_1);
    LV_FONT_DECLARE(font_awesome_14_1);

    class CustomLcdDisplay : public SpiLcdDisplay {
    private:
        lv_obj_t* text_box_container; 
        lv_obj_t* accel_label_; 
        lv_obj_t* accel_label_1;   
        lv_obj_t* accel_label_2;
        lv_obj_t* accel_label_3; 
        lv_obj_t* accel_label_4; 
        lv_obj_t* accel_label_5; 
        lv_obj_t* accel_label_6;   
        lv_obj_t* accel_label_7; 
        lv_obj_t* accel_label_8;   
        lv_obj_t* accel_label_9;
        lv_obj_t* accel_label_10;  
        lv_obj_t* accel_label_11;                                                                           
        lv_obj_t* temp_hum_data_label_; 
        lv_obj_t* Text_display;            
        lv_obj_t* img_obj_ = nullptr;  // LVGL图片对象
        bool is_visible_ = false; // 记录显示状态

        void CreateTextBox() {
            // 创建图片对象
            DisplayLockGuard lock(this);
            auto screen = lv_screen_active();
            img_obj_ = lv_img_create(screen);
            lv_obj_set_size(img_obj_, width_, height_);
            lv_obj_align(img_obj_, LV_ALIGN_TOP_MID, 0, -5);
            // 设置默认图片（可选，如“无图片”提示）
            lv_img_set_src(img_obj_, "E:/no_image.bmp");  // 需确保SD卡根目录有此默认图 //影响刷图
            lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
            // 创建文本框区域容器，改为纵向布局
            auto screen1 = lv_screen_active();
            lv_obj_set_style_text_font(screen1, fonts_.text_font, 0);
            lv_obj_set_style_text_color(screen1, current_theme_.text, 0);
            lv_obj_set_style_bg_color(screen1, current_theme_.background, 0);

            lv_obj_set_scrollbar_mode(screen1, LV_SCROLLBAR_MODE_OFF);
            
            text_box_container = lv_obj_create(screen1);
            lv_obj_set_size(text_box_container, width_, height_); // 增加容器高度以容纳三行
            lv_obj_align(text_box_container, LV_ALIGN_BOTTOM_MID, 0, -5);
            lv_obj_set_style_bg_color(text_box_container, current_theme_.chat_background, 0);
            lv_obj_set_style_border_width(text_box_container, 1, 0);
            lv_obj_set_style_border_color(text_box_container, current_theme_.border, 0);
            lv_obj_set_style_radius(text_box_container, 8, 0);
            // lv_obj_set_flex_flow(text_box_container, LV_FLEX_FLOW_COLUMN); // 改为纵向布局
            lv_obj_set_style_pad_all(text_box_container, 5, 0);
            lv_obj_add_flag(text_box_container, LV_OBJ_FLAG_HIDDEN);
             lv_obj_set_scrollbar_mode(text_box_container, LV_SCROLLBAR_MODE_OFF);

            //第一行
            temp_hum_data_label_ = lv_label_create(text_box_container);
            lv_label_set_text(temp_hum_data_label_, "");
            lv_obj_set_style_text_color(temp_hum_data_label_, current_theme_.text, 0);
            lv_obj_set_style_text_font(temp_hum_data_label_, &font_puhui_20_4, 0);
            lv_obj_set_width(temp_hum_data_label_, width_ * 0.9); // 占满容器宽度
            lv_obj_align(temp_hum_data_label_, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧
            lv_obj_set_scrollbar_mode(temp_hum_data_label_, LV_SCROLLBAR_MODE_OFF);
            lv_label_set_long_mode(temp_hum_data_label_, LV_LABEL_LONG_CLIP);
            //第二行
            accel_label_ = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_, "");
            lv_obj_set_style_text_color(accel_label_, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_, LV_ALIGN_TOP_LEFT, 10, 25); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_, LV_SCROLLBAR_MODE_OFF); 
            lv_label_set_long_mode(accel_label_, LV_LABEL_LONG_CLIP);
            //第三行 cmd = 9
            accel_label_1 = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_1, "");
            lv_obj_set_style_text_color(accel_label_1, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_1, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_1, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_1, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_1, LV_SCROLLBAR_MODE_OFF); 
            lv_label_set_long_mode(accel_label_1, LV_LABEL_LONG_CLIP);
            lv_label_set_long_mode(accel_label_1, LV_LABEL_LONG_CLIP);
            //倒数第四行
            accel_label_2 = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_2, "");
            lv_obj_set_style_text_color(accel_label_2, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_2, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_2, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_2, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_2, LV_SCROLLBAR_MODE_OFF);
            lv_label_set_long_mode(accel_label_2, LV_LABEL_LONG_CLIP);            
            //倒数第5行
            accel_label_3 = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_3, "");
            lv_obj_set_style_text_color(accel_label_3, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_3, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_3, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_3, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_3, LV_SCROLLBAR_MODE_OFF);   
            lv_label_set_long_mode(accel_label_3, LV_LABEL_LONG_CLIP); 

            //倒数第6行
            accel_label_4 = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_4, "");
            lv_obj_set_style_text_color(accel_label_4, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_4, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_4, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_4, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_4, LV_SCROLLBAR_MODE_OFF);  
            lv_label_set_long_mode(accel_label_4, LV_LABEL_LONG_CLIP); 
            //倒数第7行
            accel_label_5 = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_5, "");
            lv_obj_set_style_text_color(accel_label_5, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_5, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_5, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_5, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_5, LV_SCROLLBAR_MODE_OFF); 
            lv_label_set_long_mode(accel_label_5, LV_LABEL_LONG_CLIP); 
            //倒数第8行
            accel_label_6 = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_6, "");
            lv_obj_set_style_text_color(accel_label_6, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_6, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_6, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_6, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_6, LV_SCROLLBAR_MODE_OFF); 
            lv_label_set_long_mode(accel_label_6, LV_LABEL_LONG_CLIP);
            //倒数第9行
            accel_label_7 = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_7, "");
            lv_obj_set_style_text_color(accel_label_7, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_7, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_7, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_7, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_7, LV_SCROLLBAR_MODE_OFF); 
            lv_label_set_long_mode(accel_label_7, LV_LABEL_LONG_CLIP);
            //倒数第10行
            accel_label_8 = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_8, "");
            lv_obj_set_style_text_color(accel_label_8, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_8, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_8, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_8, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_8, LV_SCROLLBAR_MODE_OFF); 
            lv_label_set_long_mode(accel_label_8, LV_LABEL_LONG_CLIP);
            //倒数第11行
            accel_label_9 = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_9, "");
            lv_obj_set_style_text_color(accel_label_9, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_9, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_9, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_9, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_9, LV_SCROLLBAR_MODE_OFF);
            lv_label_set_long_mode(accel_label_9, LV_LABEL_LONG_CLIP);
            //倒数第12行 cmd = 12
            accel_label_10 = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_10, "");
            lv_obj_set_style_text_color(accel_label_10, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_10, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_10, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_10, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_10, LV_SCROLLBAR_MODE_OFF);
            lv_label_set_long_mode(accel_label_10, LV_LABEL_LONG_CLIP);

            //倒数第13行
            accel_label_11 = lv_label_create(text_box_container);
            lv_label_set_text(accel_label_11, "");
            lv_obj_set_style_text_color(accel_label_11, current_theme_.text, 0);
            lv_obj_set_style_text_font(accel_label_11, &font_puhui_20_4, 0);
            lv_obj_set_width(accel_label_11, width_ * 0.9); // 占满容器宽度
            lv_obj_align(accel_label_11, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(accel_label_11, LV_SCROLLBAR_MODE_OFF);
            lv_label_set_long_mode(accel_label_11, LV_LABEL_LONG_CLIP);

            // 创建SD卡状态显示标签 - 第三行
            Text_display = lv_label_create(text_box_container);
            lv_label_set_text(Text_display, "");
            lv_obj_set_style_text_color(Text_display, current_theme_.text, 0);
            lv_obj_set_style_text_font(Text_display, &font_puhui_14_1, 0);
            lv_obj_set_width(Text_display, width_ * 0.9); // 占满容器宽度
            lv_obj_align(Text_display, LV_ALIGN_TOP_LEFT, 10, 5); // 顶部左侧，由布局自动排列
            lv_obj_set_scrollbar_mode(Text_display, LV_SCROLLBAR_MODE_OFF); 
            // 为容器设置内边距，使三行之间有间隔
            lv_obj_set_style_pad_row(text_box_container, 2, 0); // 行间距
        }

    public:
        CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle, 
                        esp_lcd_panel_handle_t panel_handle,
                        int width,
                        int height,
                        int offset_x,
                        int offset_y,
                        bool mirror_x,
                        bool mirror_y,
                        bool swap_xy) 
            : SpiLcdDisplay(io_handle, panel_handle,
                        width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                        {
                            .text_font = &font_puhui_20_4,
                            .icon_font = &font_awesome_20_4,
                            .emoji_font = font_emoji_64_init(),
                        }) {
            DisplayLockGuard lock(this);
            
            // 在父类UI初始化后添加文本框
            if (container_ != nullptr) {
                CreateTextBox();
            }
        }
        //图片调用方法
        void ShowSdImage(const char* img_path) {
            DisplayLockGuard lock(this);
            if (img_obj_ == nullptr || img_path == nullptr || strlen(img_path) == 0) {
                ESP_LOGE(TAG, "图片对象未初始化或路径为空");
                return;
            }

            // 1. 拼接ESP32 VFS路径（如"E:/text1.bmp" → "/sdcard/text1.bmp"）
            char esp_path[LV_FS_MAX_PATH_LENGTH] = {0};
            if (strncmp(img_path, "E:/", 2) != 0) { // 检查路径前缀是否正确
                ESP_LOGE(TAG, "路径格式错误，需以'E:/'开头: %s", img_path);
                lv_img_set_src(img_obj_, "E:/no_image.bmp");
                return;
            }
            sprintf(esp_path, "%s/%s", MOUNT_POINT, img_path + 2);

            // 2. 检查文件是否存在（避免LVGL解析时失败）
            FILE* img_file = fopen(esp_path, "r");
            if (img_file == NULL) {
                ESP_LOGE(TAG, "图片文件不存在: %s", esp_path);
                lv_img_set_src(img_obj_, "E:/no_image.bmp");
                // 修复：仅在文件打开成功时关闭
                return;
            }
            fclose(img_file); // 文件存在，关闭句柄
            ESP_LOGI(TAG, "加载SD图片: LVGL路径=%s, VFS路径=%s", img_path, esp_path);
            lv_img_set_src(img_obj_, img_path);  //oycation
        }
        // 显示第二行文字
        void Settext(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_ != nullptr) {
                lv_label_set_text(accel_label_, text);
                lv_obj_align(accel_label_, align, x_ofs, y_ofs); // 顶部左侧
            }
        }

        void Settext1(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (temp_hum_data_label_ != nullptr) {
                lv_label_set_text(temp_hum_data_label_, text);
                lv_obj_align(temp_hum_data_label_, align, x_ofs, y_ofs); // 顶部左侧
            }
        }

        void Settext2(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_1 != nullptr) {
                lv_label_set_text(accel_label_1, text);
                lv_obj_align(accel_label_1, align, x_ofs, y_ofs); // 顶部左侧
            }
        }

        void Settext3(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_2 != nullptr) {
                lv_label_set_text(accel_label_2, text);
                lv_obj_align(accel_label_2, align, x_ofs, y_ofs); // 顶部左侧
            }
        }

        void Settext4(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_3 != nullptr) {
                lv_label_set_text(accel_label_3, text);
                lv_obj_align(accel_label_3, align, x_ofs, y_ofs); // 顶部左侧
            }
        }

        void Settext5(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_4 != nullptr) {
                lv_label_set_text(accel_label_4, text);
                lv_obj_align(accel_label_4, align, x_ofs, y_ofs); // 顶部左侧
            }
        }

        void Settext6(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_5 != nullptr) {
                lv_label_set_text(accel_label_5, text);
                lv_obj_align(accel_label_5, align, x_ofs, y_ofs); // 顶部左侧   
            }
        }

        void Settext7(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_6 != nullptr) {
                lv_label_set_text(accel_label_6, text);
                lv_obj_align(accel_label_6, align, x_ofs, y_ofs); // 顶部左侧   
            }
        }   
        
        void Settext8(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_7 != nullptr) {
                lv_label_set_text(accel_label_7, text);
                lv_obj_align(accel_label_7, align, x_ofs, y_ofs); // 顶部左侧   
            }
        }   
        
        void Settext9(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_8 != nullptr) {
                lv_label_set_text(accel_label_8, text);
                lv_obj_align(accel_label_8, align, x_ofs, y_ofs); // 顶部左侧   
            }
        }

        void Settext10(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_9 != nullptr) {
                lv_label_set_text(accel_label_9, text);
                lv_obj_align(accel_label_9, align, x_ofs, y_ofs); // 顶部左侧   
            }
        }  
        
        void Settext11(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_10 != nullptr) {
                lv_label_set_text(accel_label_10, text);
                lv_obj_align(accel_label_10, align, x_ofs, y_ofs); // 顶部左侧   
            }
        }         

        void Settext12(const char* text,lv_align_t align, int32_t x_ofs, int32_t y_ofs) {
            DisplayLockGuard lock(this);
            if (accel_label_11 != nullptr) {
                lv_label_set_text(accel_label_11, text);
                lv_obj_align(accel_label_11, align, x_ofs, y_ofs); // 顶部左侧   
            }
        } 
        //显示第三行文字
        void SetSDcardText(const char* text) {
            DisplayLockGuard lock(this);
            if (Text_display != nullptr) {
                lv_label_set_text(Text_display, text);
            }
        }

        // 显示传感器信息
        void Show() {
            DisplayLockGuard lock(this);
            lv_obj_clear_flag(text_box_container, LV_OBJ_FLAG_HIDDEN);
            is_visible_ = true;
        }
        
        // 隐藏传感器信息
        void Hide() {
            DisplayLockGuard lock(this);
            lv_obj_add_flag(text_box_container, LV_OBJ_FLAG_HIDDEN);
            is_visible_ = false;
        }

        // 新增方法：隐藏图片
        void HideImage() {
        DisplayLockGuard lock(this);
        if (img_obj_) {
            lv_obj_add_flag(img_obj_, LV_OBJ_FLAG_HIDDEN);
        }
       }

        // 新增方法：显示图片
        void ShowImage() {
            DisplayLockGuard lock(this);
            if (img_obj_) {
                lv_obj_clear_flag(img_obj_, LV_OBJ_FLAG_HIDDEN);
            }
        }



        // 查询显示状态
        bool IsVisible() const {
            return is_visible_;
        }

    };

    class XINGZHI_CUBE_1_54_TFT_MATRIXBIT_ML307 : public WifiBoard {
    private:
        i2c_master_bus_handle_t i2c_bus_;
        Button boot_button_;
        SpiLcdDisplay* display_;
        CustomLcdDisplay* custom_display_;
        PowerSaveTimer* power_save_timer_;
        PowerManager* power_manager_;
        esp_lcd_panel_io_handle_t panel_io_ = nullptr;
        esp_lcd_panel_handle_t panel_ = nullptr;
        Esp32Camera* camera_;
        Aht30Sensor* aht30_sensor_;
        Sc7a20hSensor* sc7a20h_sensor_;
        // Rp2040* Rp2040_;
        
        bool is_sd_mounted_ = false;  // SD卡挂载状态：true=已挂载，false=未挂载
        const char* sd_mount_point_ = MOUNT_POINT;  // 挂载点（如"/sdcard"，需与InitializeSDcardSpi一致）


        void InitializePowerManager() {
            power_manager_ = new PowerManager(POWER_USB_IN);//USB是否插入
            power_manager_->OnChargingStatusChanged([this](bool is_charging) {
                if (is_charging) {
                    power_save_timer_->SetEnabled(false);
                } else {
                    power_save_timer_->SetEnabled(true);
                }
            });

        }

        void InitializePowerSaveTimer() {
            power_save_timer_ = new PowerSaveTimer(-1, -1,-1);
            power_save_timer_->OnEnterSleepMode([this]() {
                ESP_LOGI(TAG, "Enabling sleep mode");
                display_->SetChatMessage("system", "");
                display_->SetEmotion("sleepy");
            });
            power_save_timer_->OnExitSleepMode([this]() {
                display_->SetChatMessage("system", "");
                display_->SetEmotion("neutral");
            });
            power_save_timer_->OnShutdownRequest([this]() {
                ESP_LOGI(TAG, "Shutting down");
                rtc_gpio_set_level(NETWORK_MODULE_POWER_IN, 0);
                // 启用保持功能，确保睡眠期间电平不变
                rtc_gpio_hold_en(NETWORK_MODULE_POWER_IN);
                esp_lcd_panel_disp_on_off(panel_, false); //关闭显示
                esp_deep_sleep_start();
            });
            power_save_timer_->SetEnabled(true);
        }



        // 添加一个标志位来记录扫描结果
        esp_err_t err;
        bool is_device_41_found = false;
        bool is_device_18_found = false;
      
   

        void InitializeI2c() {
            // 初始化I2C外设
            i2c_master_bus_config_t i2c_bus_cfg = {
                .i2c_port = I2C_NUM_1,  // 明确使用I2C_NUM_1宏定义，更易读
                .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
                .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
                .clk_source = I2C_CLK_SRC_DEFAULT,
                .glitch_ignore_cnt = 7,
                .intr_priority = 0,
                .trans_queue_depth = 0,
                .flags = {
                    .enable_internal_pullup = 1,  // 启用内部上拉电阻
                },
            };
            
            
            // 创建I2C主总线，使用更灵活的错误处理
            esp_err_t err = i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create I2C master bus (err=0x%x)", err);
                return;  // 总线创建失败，直接返回
            }

            //I2C设备扫描（修正变量名，优化超时时间）
            bool is_device_41_found = false;
            bool is_device_18_found = false;
            for (uint8_t addr = 1; addr < 127; addr++) {
                err = i2c_master_probe(i2c_bus_, addr, 100);  // 超时时间改为100ms
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Found device at address 0x%02X", addr);  // 使用ESP日志宏，更规范
                    // 恢复设备地址检查逻辑
                    if (addr == 0x41) {
                        is_device_41_found = true;
                    }
                    if (addr == 0x18) {
                        is_device_18_found = true;
                    }
                }
            }
            ESP_LOGI(TAG, "I2C initialization completed successfully");
        }

        void InitializeAHT30Sensor() {
            uint8_t reciving = 0;
            uint8_t recivingligh = 0;
            // 初始化传感器
            aht30_sensor_ = new Aht30Sensor(i2c_bus_);
            esp_err_t err = aht30_sensor_->Initialize();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize AHT30 sensor (err=0x%x)", err);
                return;
            }

            // // 设置温湿度数据回调
            // aht30_sensor_->SetAht30SensorCallback([this](float temp, float hum) {
            //     UpdateAht30SensorDisplay(temp, hum);
            // });

            // 启动周期性读取（每秒一次）
            err = aht30_sensor_->StartReading(3000);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start periodic readings (err=0x%x)", err);
            }

        }

        // // 更新温湿度显示
        // void UpdateAht30SensorDisplay(float temp, float hum) {
        //     if (custom_display_) {
        //         custom_display_->SetAht30SensoryText(temp, hum);
        //     }
        // }

        void InitializeSC7A20HSensor() {
            // 初始化传感器
            sc7a20h_sensor_ = new Sc7a20hSensor(i2c_bus_);
            esp_err_t err = sc7a20h_sensor_->Initialize();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "初始化SC7A20H传感器失败 (err=0x%x)", err);
                return;
            }

            // // 设置加速度数据回调
            // sc7a20h_sensor_->SetAccelerationCallback([this](float x, float y, float z) {
            //     UpdateAccelerationDisplay(x, y, z);
            // });

            // 启动周期性读取（每100ms一次）
            err = sc7a20h_sensor_->StartReading(3000);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "启动周期性读取失败 (err=0x%x)", err);
            }
        }

        // 更新加速度显示
        // void UpdateAccelerationDisplay(float x, float y, float z) {
        //     if (custom_display_) {
        //         char buffer[50];
        //         sprintf(buffer, "X:%.2f Y:%.2f Z:%.2f", x, y, z);
        //         custom_display_->SetAccelerationText(buffer);
        //     }
        // }

        void InitializeSpi() {
            spi_bus_config_t buscfg = {};
            buscfg.mosi_io_num = DISPLAY_SDA;
            buscfg.miso_io_num = GPIO_NUM_NC;
            buscfg.sclk_io_num = DISPLAY_SCL;
            buscfg.quadwp_io_num = GPIO_NUM_NC;
            buscfg.quadhd_io_num = GPIO_NUM_NC;
            buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
            ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
        }

        void InitializeSDcardSpi() {
            // 初始化SPI总线配置
            spi_bus_config_t bus_cnf = {
                .mosi_io_num = SDCARD_PIN_MOSI,
                .miso_io_num = SDCARD_PIN_MISO,
                .sclk_io_num = SDCARD_PIN_CLK,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
                .max_transfer_sz = 400000,
            };
            // 初始化SPI总线
            esp_err_t err = spi_bus_initialize(SDCARD_SPI_HOST, &bus_cnf, SPI_DMA_CH_AUTO);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "SPI总线初始化失败: %s", esp_err_to_name(err));
                return;
            }
            
            // 配置SD卡设备
            static sdspi_device_config_t slot_cnf = {
                .host_id = SDCARD_SPI_HOST,
                .gpio_cs = SDCARD_PIN_CS,
                .gpio_cd = SDSPI_SLOT_NO_CD,
                .gpio_wp = GPIO_NUM_NC,
                .gpio_int = GPIO_NUM_NC,                
            };
            
            // 配置FAT文件系统挂载选项
            esp_vfs_fat_sdmmc_mount_config_t mount_cnf = {
                .format_if_mount_failed = false,
                .max_files = 5,
                .allocation_unit_size = 16 * 1024,
                .disk_status_check_enable = true, // 启用磁盘状态检查
            };

            
            // 声明SD卡对象
            sdmmc_card_t* card = NULL;

            // 先将宏结果赋值给变量，再取地址
            sdmmc_host_t host = SDSPI_HOST_DEFAULT();
            host.max_freq_khz = 20000;  // 设置主机最大频率 

            err = esp_vfs_fat_sdspi_mount(sd_mount_point_, &host, &slot_cnf, &mount_cnf, &card);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "SD卡挂载失败: %s", esp_err_to_name(err));
                // custom_display_->SetSDcardText("SD卡挂载失败");
                is_sd_mounted_ = false;  // 标记为未挂载
                return;
            } else {
                ESP_LOGI(TAG, "SD卡挂载成功");
                // custom_display_->SetSDcardText("SD卡挂载成功");
                is_sd_mounted_ = true;  
                sdmmc_card_print_info(stdout, card);  // 打印SD卡信息
                
            }              

        }

        void InitializeButtons() {
            boot_button_.OnClick([this]() {
                power_save_timer_->WakeUp();
                auto& app = Application::GetInstance();
                // if (GetNetworkType() == NetworkType::WIFI) {
                    if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                        // auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
                           ResetWifiConfiguration();
                    }
                // }
                app.ToggleChatState();
            });

            boot_button_.OnDoubleClick([this]() {
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                } else {
                    // 先判断SD卡是否挂载
                    if (!is_sd_mounted_) {
                        ESP_LOGE(TAG, "SD卡未挂载，无法显示图片");
                        custom_display_->SetSDcardText("SD未挂载，无法显示图");
                        return;
                    }
                    // 切换显示/隐藏传感器文本框 + 显示SD图片
                    if (custom_display_->IsVisible()) {
                        custom_display_->Hide();
                        custom_display_->HideImage();     // 隐藏图片p
                        ESP_LOGI(TAG,"隐藏传感器信息");
                    } else {
                        custom_display_->Show();
                        custom_display_->ShowImage();     // 显示图片（如果需要） 
                        ESP_LOGI(TAG,"显示传感器信息");
                        //显示SD卡中的图片（路径需与实际一致）
                        custom_display_->ShowSdImage("E:/smell1.bmp");  // 假设SD卡根目录有test.bmp
                    }  
                }
            });
        }

        //oycation
        void Initializeuart() {
        // 初始化 UART
        uart_config_t uart_config = {
            .baud_rate = 9600,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        };  
        UartSender::Initialize(UART_NUM_1, uart_config); //初始化 UART 并创建单例
        // 获取单例并发送数据
        UartSender& sender = UartSender::GetInstance(); //获取单例引用 oycation888 通过UartSender::GetInstance()获取实例，调用成员函数(发送函数)
    }


// 打开文件时创建缓存
static void* lv_fs_sd_open(lv_fs_drv_t* drv, const char* path, lv_fs_mode_t mode) {
    char esp_path[LV_FS_MAX_PATH_LENGTH] = {0};
    sprintf(esp_path, "%s/%s", MOUNT_POINT, path + 2);

    const char* c_mode = NULL;
    if (mode == LV_FS_MODE_RD) c_mode = "rb";
    else if (mode == LV_FS_MODE_WR) c_mode = "wb";
    else if (mode == (LV_FS_MODE_RD | LV_FS_MODE_WR)) c_mode = "rb+";

    FILE* file = fopen(esp_path, c_mode);
    if (!file) {
        ESP_LOGE(TAG, "Failed to open file: %s", esp_path);
        return NULL;
    }

    // 分配带缓存的文件结构
    sd_file_cache_t* fc = (sd_file_cache_t*)malloc(sizeof(sd_file_cache_t));
    if (!fc) {
        fclose(file);
        ESP_LOGE(TAG, "Failed to allocate file cache");
        return NULL;
    }

    fc->fp = file;
    fc->len = 0;
    fc->pos = 0;

    return fc;  // 返回带缓存的句柄
}

// 关闭文件时释放缓存
static lv_fs_res_t lv_fs_sd_close(lv_fs_drv_t* drv, void* file_p) {
    sd_file_cache_t* fc = (sd_file_cache_t*)file_p;
    if (fclose(fc->fp) != 0) {
        free(fc);
        return LV_FS_RES_FS_ERR;
    }
    free(fc);
    return LV_FS_RES_OK;
}

// 带缓存的读取
static lv_fs_res_t lv_fs_sd_read(lv_fs_drv_t* drv, void* file_p, void* buf, uint32_t btr, uint32_t* br) {
    sd_file_cache_t* fc = (sd_file_cache_t*)file_p;
    uint8_t* out = (uint8_t*)buf;
    size_t remaining = btr;
    *br = 0;

    while (remaining > 0) {
        // 缓存为空时，从SD卡读取大块数据
        if (fc->pos >= fc->len) {
            size_t read_size = fread(fc->buffer, 1, SD_READ_CACHE_SIZE, fc->fp);
            if (read_size == 0) {
                if (feof(fc->fp)) break;
                if (ferror(fc->fp)) {
                    ESP_LOGE(TAG, "Failed to read file");
                    clearerr(fc->fp);
                    return LV_FS_RES_FS_ERR;
                }
                continue;
            }
            fc->len = read_size;
            fc->pos = 0;
        }

        // 从缓存复制到目标
        size_t copy_size = MIN(remaining, fc->len - fc->pos);
        memcpy(out, &fc->buffer[fc->pos], copy_size);

        out += copy_size;
        fc->pos += copy_size;
        remaining -= copy_size;
        *br += copy_size;
    }

    return LV_FS_RES_OK;
}

// 4. 写入文件回调（符合 lv_fs_drv_t::write_cb 签名）
static lv_fs_res_t lv_fs_sd_write(lv_fs_drv_t* drv, void* file_p, const void* buf, uint32_t btw, uint32_t* bw) {
    FILE* file = (FILE*)file_p;
    // 调用 fwrite 写入数据，btw=要写的字节数，bw=实际写的字节数
    size_t written_bytes = fwrite(buf, 1, btw, file);
    if (bw != NULL) {
        *bw = written_bytes;
    }

    if (ferror(file)) {
        ESP_LOGE(TAG, "Failed to write file");
        clearerr(file);
        return LV_FS_RES_FS_ERR;
    }
    return LV_FS_RES_OK;
}

// 其他回调也需要修改，例如 seek
static lv_fs_res_t lv_fs_sd_seek(lv_fs_drv_t* drv, void* file_p, uint32_t pos, lv_fs_whence_t whence) {
    sd_file_cache_t* fc = (sd_file_cache_t*)file_p;
    // 清空缓存，因为 seek 后缓存不再有效
    fc->len = 0;
    fc->pos = 0;

    int c_whence = 0;
    switch (whence) {
        case LV_FS_SEEK_SET: c_whence = SEEK_SET; break;
        case LV_FS_SEEK_CUR: c_whence = SEEK_CUR; break;
        case LV_FS_SEEK_END: c_whence = SEEK_END; break;
        default: return LV_FS_RES_INV_PARAM;
    }

    if (fseek(fc->fp, pos, c_whence) != 0) {
        return LV_FS_RES_FS_ERR;
    }
    return LV_FS_RES_OK;
}

// tell 回调也需要修改
static lv_fs_res_t lv_fs_sd_tell(lv_fs_drv_t* drv, void* file_p, uint32_t* pos_p) {
    sd_file_cache_t* fc = (sd_file_cache_t*)file_p;
    long pos = ftell(fc->fp);
    if (pos < 0) return LV_FS_RES_FS_ERR;
    *pos_p = (uint32_t)pos;
    return LV_FS_RES_OK;
}
        //图片显示
        void InitializeSt7789Display() {
            
            // 初始化LVGL文件系统：将ESP32的VFS映射到LVGL的"SD:"路径
            lv_fs_drv_t fs_drv;
            lv_fs_drv_init(&fs_drv);  // 初始化驱动结构体
            // vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGD(TAG, "Install panel IO");
            esp_lcd_panel_io_spi_config_t io_config = {};
            io_config.cs_gpio_num = DISPLAY_CS; //gpio 21
            io_config.dc_gpio_num = DISPLAY_DC; //gpio 38
            io_config.spi_mode = 3;
            io_config.pclk_hz = 80 * 1000 * 1000;
            io_config.trans_queue_depth = 10;
            io_config.lcd_cmd_bits = 8;
            io_config.lcd_param_bits = 8;
            ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io_));

            ESP_LOGD(TAG, "Install LCD driver");
            esp_lcd_panel_dev_config_t panel_config = {};
            panel_config.reset_gpio_num = DISPLAY_RES;  //gpio 21
            panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
            panel_config.bits_per_pixel = 16;

            ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));
            ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
            ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
            ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
            ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
            ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));

            fs_drv.letter = 'E';      // LVGL访问路径前缀："E:/文件名.bmp"
            fs_drv.cache_size = 0;    // 不需要缓存（SD 卡读写直接通过 VFS）
            fs_drv.user_data = NULL;  // 无额外用户数据（如需传挂载点可在这里设置）


            // 关键：赋值自定义的适配层回调（替换原来的 lv_fs_xxx）
            fs_drv.open_cb = lv_fs_sd_open;    // 打开文件
            fs_drv.read_cb = lv_fs_sd_read;    // 读取文件
            fs_drv.write_cb = lv_fs_sd_write;  // 写入文件
            fs_drv.seek_cb = lv_fs_sd_seek;    // 定位指针
            fs_drv.tell_cb = lv_fs_sd_tell;    // 获取指针位置
            fs_drv.close_cb = lv_fs_sd_close;  // 关闭文件
            
            // 将驱动注册到LVGL
            lv_fs_drv_register(&fs_drv);            
            ESP_LOGI(TAG, "LVGL文件系统映射完成，可通过 E:/ 访问SD卡");


            custom_display_ = new CustomLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
            display_ = custom_display_;
        }

        void InitializeIot() {
            auto& thing_manager = iot::ThingManager::GetInstance();
            thing_manager.AddThing(iot::CreateThing("Speaker"));
            thing_manager.AddThing(iot::CreateThing("Screen"));
            thing_manager.AddThing(iot::CreateThing("Battery"));
            thing_manager.AddThing(iot::CreateThing("Sevor"));
            // thing_manager.AddThing(iot::CreateThing("Lamp"));


        }

        void InitializeGpio() {                 //初始化背光拉高
            gpio_config_t zxc = {};
            zxc.intr_type = GPIO_INTR_DISABLE;
            zxc.mode = GPIO_MODE_OUTPUT;
            zxc.pin_bit_mask = (1ULL << GPIO_NUM_11);
            zxc.pull_down_en = GPIO_PULLDOWN_DISABLE; 
            zxc.pull_up_en = GPIO_PULLUP_DISABLE;     
            gpio_config(&zxc);
            gpio_set_level(GPIO_NUM_11, 1); 

        }

        void InitializeCamera() {
            camera_config_t config = {};
            config.ledc_channel = LEDC_CHANNEL_2;  // LEDC通道选择  用于生成XCLK时钟 但是S3不用
            config.ledc_timer = LEDC_TIMER_2; // LEDC timer选择  用于生成XCLK时钟 但是S3不用
            config.pin_d0 = CAMERA_PIN_D0;
            config.pin_d1 = CAMERA_PIN_D1;
            config.pin_d2 = CAMERA_PIN_D2;
            config.pin_d3 = CAMERA_PIN_D3;
            config.pin_d4 = CAMERA_PIN_D4;
            config.pin_d5 = CAMERA_PIN_D5;
            config.pin_d6 = CAMERA_PIN_D6;
            config.pin_d7 = CAMERA_PIN_D7;
            config.pin_xclk = CAMERA_PIN_XCLK;
            config.pin_pclk = CAMERA_PIN_PCLK;
            config.pin_vsync = CAMERA_PIN_VSYNC;
            config.pin_href = CAMERA_PIN_HREF;
            config.pin_sccb_sda = -1;   // 这里写-1 表示使用已经初始化的I2C接口
            config.pin_sccb_scl = CAMERA_PIN_SIOC;
            config.sccb_i2c_port = 1;
            config.pin_pwdn = CAMERA_PIN_PWDN;
            config.pin_reset = CAMERA_PIN_RESET;
            config.xclk_freq_hz = XCLK_FREQ_HZ;
            config.pixel_format = PIXFORMAT_RGB565;
            config.frame_size = FRAMESIZE_VGA;
            config.jpeg_quality = 12;
            config.fb_count = 1;
            config.fb_location = CAMERA_FB_IN_PSRAM;
            config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

            camera_ = new Esp32Camera(config);
        }

    public:
        XINGZHI_CUBE_1_54_TFT_MATRIXBIT_ML307() :
        
            // DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, 4096),
            boot_button_(BOOT_BUTTON_GPIO) { 
            // vTaskDelay(pdMS_TO_TICKS(3000));  
            InitializeI2c();    
            InitializeGpio();
            Initializeuart();
            InitializePowerManager();
            InitializePowerSaveTimer();
            InitializeAHT30Sensor();  
            InitializeSC7A20HSensor();
            InitializeButtons();
            InitializeSpi();
            InitializeSDcardSpi();  
            InitializeSt7789Display();            

            InitializeIot();
            InitializeCamera();
            
        }

        virtual AudioCodec* GetAudioCodec() override {
            static Es8311AudioCodec audio_codec(
                i2c_bus_, 
                I2C_NUM_0,
                AUDIO_INPUT_SAMPLE_RATE, 
                AUDIO_OUTPUT_SAMPLE_RATE,
                AUDIO_I2S_GPIO_MCLK, 
                AUDIO_I2S_GPIO_BCLK, 
                AUDIO_I2S_GPIO_WS, 
                AUDIO_I2S_GPIO_DOUT, 
                AUDIO_I2S_GPIO_DIN,
                AUDIO_CODEC_PA_PIN,      //功放输出21dB
                AUDIO_CODEC_ES8311_ADDR, 
                AUDIO_INPUT_REFERENCE);
                return &audio_codec;
        }

            virtual Display* GetDisplay() override {
            return display_;
        }

        virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
            static bool last_discharging = false;
            charging = power_manager_->IsCharging();
            discharging = power_manager_->IsDischarging();
            if (discharging != last_discharging) {
                power_save_timer_->SetEnabled(discharging);
                last_discharging = discharging;
            }
            level = power_manager_->GetBatteryLevel();
            return true;
        }

        virtual void SetPowerSaveMode(bool enabled) override {
            if (!enabled) {
                power_save_timer_->WakeUp();
            }
            WifiBoard::SetPowerSaveMode(enabled);
        }

        virtual Camera* GetCamera() override {
            return camera_;
        }


    };

    DECLARE_BOARD(XINGZHI_CUBE_1_54_TFT_MATRIXBIT_ML307);
