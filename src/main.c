// for gtk app
#include <gtk/gtk.h> // Use to make a software in c
#include <string.h> //
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h> // Include for mkdir
#include <errno.h>    // Include for errno
#include <ctype.h>
#include <stdbool.h>

// for conversion
#include <stdint.h>
#include <png.h>

#define SAMPLE_RATE 44100
#define DURATION 0.05 // Duration for each pixel in seconds
#define BUFFER_SIZE 4096 // Buffer size for writing samples
#define BITS_PER_SAMPLE 32

// Global variable for the progress bar
GtkProgressBar *progress_bar;
int g_selected_sample_rate = 44100;
int g_selected_mode = 0;

// Define the AppData structure
typedef struct {
    GtkLabel *error_label;
    GtkComboBoxText *samplerate_combo_sample;
    GtkComboBoxText *samplerate_combo_mode;
} AppData;

typedef struct {
    GtkProgressBar *progress_bar_img; // progress bar
    GtkLabel *conversion_label; // progress box
    GtkLabel *con_progress_text; // covert status
    GtkLabel *output_box_audio; // output box
    GtkBuilder *builder;
} Status_img_wav, Status_wav_img;

// --------------------------------------------------------------------------------------------------------
// WAV file header structure
typedef struct {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t fmt_tag;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
} WavHeader;

// WAV file header structure
typedef struct {
    char chunkID[4];         // "RIFF"
    uint32_t chunkSize;      // File size - 8 bytes
    char format[4];          // "WAVE"
    char subchunk1ID[4];     // "fmt "
    uint32_t subchunk1Size;  // Size of fmt chunk (16 for PCM)
    uint16_t audioFormat;    // Audio format (1 = PCM, others = compressed)
    uint16_t numChannels;    // Number of channels
    uint32_t sampleRate;     // Sampling frequency
    uint32_t byteRate;       // Bytes per second
    uint16_t blockAlign;     // Bytes per sample slice
    uint16_t bitsPerSample;  // Bits per sample
    char subchunk2ID[4];     // "data"
    uint32_t subchunk2Size;  // Size of the data chunk
} WAVHeader;
// --------------------------------------------------------------------------------------------------------

// Function to read the WAV file header
void read_wav_header(FILE *file, WavHeader *header) {
    fread(header, sizeof(WavHeader), 1, file);
}

// Function to write a WAV file header
void write_wav_header(FILE *file, int num_samples) {
    WavHeader header;
    int file_size = num_samples * sizeof(int16_t) + sizeof(WavHeader) - 8;
    int data_size = num_samples * sizeof(int16_t);

    // Fill WAV header
    memcpy(header.riff, "RIFF", 4);
    header.file_size = file_size;
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    header.fmt_size = 16;
    header.fmt_tag = 1; // PCM
    header.channels = 1; // Mono
    header.sample_rate = g_selected_sample_rate; // SAMPLE_RATE
    header.byte_rate = g_selected_sample_rate * sizeof(int16_t); //SAMPLE_RATE
    header.block_align = sizeof(int16_t);
    header.bits_per_sample = 16;
    memcpy(header.data, "data", 4);
    header.data_size = data_size;

    // Write header
    fwrite(&header, sizeof(WavHeader), 1, file);
}

// Function to write pixel intensity as audio sample
void generate_audio_samples(FILE *file, int16_t *samples, int num_samples) {
    fwrite(samples, sizeof(int16_t), num_samples, file); // Write all samples at once
}

// Function to update the GTK progress bar
void update_progress_bar(GtkProgressBar *progress_bar, gpointer user_data, double fraction) {
    // Ensure the progress value is between 0.0 and 1.0
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0; // Ensure it does not exceed 1.0

    // Update the progress bar fraction
    gtk_progress_bar_set_fraction(progress_bar, fraction);
    
    // Update the text shown on the progress bar
    char text[10];
    snprintf(text, sizeof(text), "%.2f%%", fraction * 100.0); // Use 'fraction' instead of 'progress'
    gtk_progress_bar_set_text(progress_bar, text);
    
    // Force the UI to update
    gtk_widget_show(GTK_WIDGET(progress_bar));          // Cast to GtkWidget*
    gtk_widget_queue_draw(GTK_WIDGET(progress_bar));    // Cast to GtkWidget*
}

// Function to read PNG file and convert to grayscale intensity - img - wav -
int read_png_file(const char *filename, int *width, int *height, uint8_t **pixels) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Couldn't open file %s for reading.\n", filename);
        return -1;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        fprintf(stderr, "Error: Couldn't initialize PNG read struct.\n");
        return -1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        fprintf(stderr, "Error: Couldn't initialize PNG info struct.\n");
        return -1;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        fprintf(stderr, "Error: Couldn't set PNG jump buffer.\n");
        return -1;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    *width = png_get_image_width(png, info);
    *height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16)
        png_set_strip_16(png);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY_ALPHA || color_type == PNG_COLOR_TYPE_RGBA)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    *pixels = (uint8_t *)malloc(*width * *height * 4);
    if (*pixels == NULL) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        fprintf(stderr, "Error: Couldn't allocate memory for pixels.\n");
        return -1;
    }

    png_bytep rows[*height];
    for (int y = 0; y < *height; y++) {
        rows[y] = (*pixels) + y * (*width) * 4;
    }

    png_read_image(png, rows);

    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    return 0;
}

// Function to write a PNG file from pixel data - wav - img -
void write_png_file(const char *filename, int width, int height, uint8_t *pixels) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Couldn't open file %s for writing.\n", filename);
        return;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        fprintf(stderr, "Error: Couldn't initialize PNG write struct.\n");
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        fprintf(stderr, "Error: Couldn't initialize PNG info struct.\n");
        return;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        fprintf(stderr, "Error: Couldn't set PNG jump buffer.\n");
        return;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    png_bytep row = (png_bytep)malloc(width);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            row[x] = pixels[y * width + x]; // Use grayscale intensity directly
        }
        png_write_row(png, row);
    }

    free(row);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
}

// -------------------------------------------------------------------------------------------------------- png

// Callback function for file chooser to process the selected PNG file
void on_file_selected_img(GtkFileChooserButton *chooser, GtkLabel *error_label) {
    // Get the file path of the selected file
    char *file_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));

    // Check if the file has a ".png" extension
    if (g_str_has_suffix(file_path, ".png")) {
        // Define the destination directory and file name
        const char *dest_folder = "assets/input/image";
        const char *dest_filename = "image.png";
        char dest_path[512];

        // Create the destination folder if it doesn't exist
        struct stat st = {0};
        if (stat(dest_folder, &st) == -1) {
            mkdir(dest_folder); // Create directory
        }

        // Create the full destination path
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_folder, dest_filename);

        // Debugging: Print the file paths
        g_print("Attempting to copy file from %s to %s\n", file_path, dest_path);

        // Use GIO to copy the selected file to the destination
        GError *error = NULL;
        GFile *src_file = g_file_new_for_path(file_path);
        GFile *dest_file = g_file_new_for_path(dest_path);

        // Copy the file and replace if it exists
        g_file_copy(src_file, dest_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);

        if (error) {
            // Print detailed error information
            g_print("Error copying the file: %s\n", error->message);
            // Show error message with colored text
            gtk_widget_show(GTK_WIDGET(error_label));
            char *copy_error_message = g_strdup_printf("<span foreground=\"red\">Error copying the file: %s</span>", error->message);
            gtk_label_set_markup(error_label, copy_error_message);
            g_free(copy_error_message);
            g_error_free(error);
        } else {
            g_print("File successfully copied to %s\n", dest_path);
            // Optionally hide the error label if the operation is successful
            // gtk_widget_hide(GTK_WIDGET(error_label)); //in case hide

            // Show success message with colored text
            gtk_widget_show(GTK_WIDGET(error_label));
            char *file_error_message = g_strdup_printf("<span foreground=\"#007a0e\">File successfully Added.</span>");
            gtk_label_set_markup(error_label, file_error_message);
            g_free(file_error_message);
        }

        // Clean up GFile objects
        g_object_unref(src_file);
        g_object_unref(dest_file);
    } else {
        // Show error message with colored text
        gtk_widget_show(GTK_WIDGET(error_label));
        char *file_error_message = g_strdup_printf("<span foreground=\"red\">Please select a .png file</span>");
        gtk_label_set_markup(error_label, file_error_message);
        g_free(file_error_message);

        // Hide the error label after 5 seconds
        g_timeout_add_seconds(5, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(error_label));
    }

    // Free the file path string
    g_free(file_path);
}

// -------------------------------------------------------------------------------------------------------- wav

// Callback function for file chooser to process the selected WAV file
void on_file_selected_wav(GtkFileChooserButton *chooser, GtkLabel *error_label) {
    // Get the file path of the selected file
    char *file_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));

    // Check if the file has a ".png" extension
    if (g_str_has_suffix(file_path, ".wav")) {
        // Define the destination directory and file name
        const char *dest_folder = "assets/input/audio";
        const char *dest_filename = "input.wav";
        char dest_path[512];

        // Create the destination folder if it doesn't exist
        struct stat st = {0};
        if (stat(dest_folder, &st) == -1) {
            mkdir(dest_folder); // Create directory
        }

        // Create the full destination path
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_folder, dest_filename);

        // Debugging: Print the file paths
        g_print("Attempting to copy file from %s to %s\n", file_path, dest_path);

        // Use GIO to copy the selected file to the destination
        GError *error = NULL;
        GFile *src_file = g_file_new_for_path(file_path);
        GFile *dest_file = g_file_new_for_path(dest_path);

        // Copy the file and replace if it exists
        g_file_copy(src_file, dest_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);

        if (error) {
            // Print detailed error information
            g_print("Error copying the file: %s\n", error->message);
            // Show error message with colored text
            gtk_widget_show(GTK_WIDGET(error_label));
            char *copy_error_message = g_strdup_printf("<span foreground=\"red\">Error copying the file: %s</span>", error->message);
            gtk_label_set_markup(error_label, copy_error_message);
            g_free(copy_error_message);
            g_error_free(error);
        } else {
            g_print("File successfully copied to %s\n", dest_path);
            // Optionally hide the error label if the operation is successful
            // gtk_widget_hide(GTK_WIDGET(error_label)); //in case hide

            // Show success message with colored text
            gtk_widget_show(GTK_WIDGET(error_label));
            char *file_error_message = g_strdup_printf("<span foreground=\"#007a0e\">File successfully Added.</span>");
            gtk_label_set_markup(error_label, file_error_message);
            g_free(file_error_message);
        }

        // Clean up GFile objects
        g_object_unref(src_file);
        g_object_unref(dest_file);
    } else {
        // Show error message with colored text
        gtk_widget_show(GTK_WIDGET(error_label));
        char *file_error_message = g_strdup_printf("<span foreground=\"red\">Please select a .wav file</span>");
        gtk_label_set_markup(error_label, file_error_message);
        g_free(file_error_message);

        // Hide the error label after 5 seconds
        g_timeout_add_seconds(5, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(error_label));
    }

    // Free the file path string
    g_free(file_path);
}

// Callback to update the label with the selected sample rate
void update_sample_rate_label(GtkComboBoxText *combo_box, GtkLabel *label) {
    const char *selected_sample_rate = gtk_combo_box_text_get_active_text(combo_box);
    if (selected_sample_rate != NULL) {
        // Buffer to hold the numeric part of the sample rate
        char number[16] = {0}; // Buffer to hold the extracted number as a string
        int i = 0;

        // Loop through the selected_sample_rate to extract digits
        while (*selected_sample_rate && i < sizeof(number) - 1) {
            if (isdigit(*selected_sample_rate)) {
                number[i++] = *selected_sample_rate; // Copy the digit
            }
            selected_sample_rate++;
        }
        number[i] = '\0'; // Null-terminate the string

        // Convert the extracted number string to an integer
        g_selected_sample_rate = atoi(number);

        // Update the label text to show the selected sample rate
        char label_text[128];
        snprintf(label_text, sizeof(label_text), "Selected Sample Rate: %s", number);
        gtk_label_set_text(label, label_text);

        // Print to console for debugging
        g_print("Updated label with sample rate: %d\n", g_selected_sample_rate);
    } else {
        gtk_label_set_text(label, "No sample rate selected");
    }
}

// Callback to update the mode with the selected mod
void update_mode_label(GtkComboBoxText *combo_box, GtkLabel *label) {
    // Get the selected text from the combo box
    const char *selected_mode = gtk_combo_box_text_get_active_text(combo_box);

    if (selected_mode != NULL) {
        // Map the selected mode to the corresponding value
        if (strcmp(selected_mode, "Array") == 0) {
            g_selected_mode = 4;
        } else if (strcmp(selected_mode, "Linked List") == 0) {
            g_selected_mode = 1;
        } else if (strcmp(selected_mode, "Stack") == 0) {
            g_selected_mode = 2;
        } else if (strcmp(selected_mode, "Queue") == 0) {
            g_selected_mode = 3;
        } else {
            g_selected_mode = 0; // Default value if mode is unknown
        }

        // Update the label text to show the selected mode
        char label_text[128];
        snprintf(label_text, sizeof(label_text), "Selected Mode: %s.", selected_mode);
        gtk_label_set_text(label, label_text);

        // Print to console for debugging
        g_print("Updated label with mode: %s (Code: %d)\n", selected_mode, g_selected_mode);
    } else {
        // Handle the case where no mode is selected
        gtk_label_set_text(label, "No mode selected");
        g_selected_mode = 0;
    }
}

// =========================================================================================================== img - wav
void set_text(GtkBuilder *builder) {
    FILE *file = fopen("assets/output/audio/output.wav", "rb");
    if (!file) {
        printf("Error: Could not open file 'assets/output/audio/output.wav'.\n");
        return;
    }

    WAVHeader header;

    // Read the WAV header
    if (fread(&header, sizeof(WAVHeader), 1, file) != 1) {
        printf("Error: Could not read WAV header.\n");
        fclose(file);
        return;
    }

    // Validate that the file is a WAV file
    if (strncmp(header.chunkID, "RIFF", 4) != 0 || strncmp(header.format, "WAVE", 4) != 0) {
        printf("Error: The file is not a valid WAV file.\n");
        fclose(file);
        return;
    }

    // Prepare the text to be displayed in the GtkTextView
    char text[1024];
    snprintf(text, sizeof(text),
        "WAV File Information:\n"
        "-------------------------------------------------\n"
        "| %-20s \t\t| %.4s\t\t\t|\n"
        "| %-20s \t\t| %u bytes\t|\n"
        "| %-20s \t\t| %.4s\t\t\t|\n"
        "| %-20s \t| %.4s\t\t\t|\n"
        "| %-20s \t| %u (1 = PCM)\t\t|\n"
        "| %-20s \t\t| %u\t\t\t\t|\n"
        "| %-20s \t| %u Hz\t\t|\n"
        "| %-20s \t\t| %u bytes/sec\t|\n"
        "| %-20s \t\t| %u bytes\t\t\t|\n"
        "| %-20s \t| %u\t\t\t\t|\n"
        "| %-20s \t| %.4s\t\t\t|\n"
        "| %-20s \t\t| %u bytes\t|\n",
        "Chunk ID", header.chunkID,
        "File Size", header.chunkSize + 8,
        "Format", header.format,
        "Subchunk1 ID", header.subchunk1ID,
        "Audio Format", header.audioFormat,
        "Channels", header.numChannels,
        "Sample Rate", header.sampleRate,
        "Byte Rate", header.byteRate,
        "Block Align", header.blockAlign,
        "Bits Per Sample", header.bitsPerSample,
        "Subchunk2 ID", header.subchunk2ID,
        "Data Size", header.subchunk2Size);

    // Get the text view widget by its ID from the builder
    GtkWidget *text_view = GTK_WIDGET(gtk_builder_get_object(builder, "text_vew_data"));

    // Get the text buffer associated with the text view
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));

    // Set the formatted text in the buffer
    gtk_text_buffer_set_text(buffer, text, -1);

    // Disable editing
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
}

// Image to audio ==========================================================================================

// Node for Linked List, Stack, and Queue
typedef struct Node {
    int16_t data;
    struct Node *next;
} Node;

// Linked List functions
void append_to_list(Node **head, int16_t value) {
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->data = value;
    new_node->next = NULL;

    if (*head == NULL) {
        *head = new_node;
    } else {
        Node *temp = *head;
        while (temp->next) {
            temp = temp->next;
        }
        temp->next = new_node;
    }
}

// Stack functions
void push_to_stack(Node **stack, int16_t value) {
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->data = value;
    new_node->next = *stack;
    *stack = new_node;
}

// Queue functions
void enqueue_to_queue(Node **rear, Node **front, int16_t value) {
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->data = value;
    new_node->next = NULL;

    if (*rear == NULL) {
        *front = *rear = new_node;
    } else {
        (*rear)->next = new_node;
        *rear = new_node;
    }
}

// Write all samples from a structure to the file
void write_samples_from_structure(FILE *audio_file, Node *head, int mode) {
    Node *current = head;
    if (mode == 2) { // If stack, reverse the order
        Node *prev = NULL, *next = NULL;
        while (current) {
            next = current->next;
            current->next = prev;
            prev = current;
            current = next;
        }
        current = prev;
    }

    while (current) {
        fwrite(&(current->data), sizeof(int16_t), 1, audio_file);
        Node *temp = current;
        current = current->next;
        free(temp); // Free memory
    }
}

int main_image_to_audio(GtkProgressBar *progress_bar, gpointer user_data, GtkBuilder *builder){
    // convert with mode
    // Check if progress_bar is valid
    if (!GTK_IS_PROGRESS_BAR(progress_bar)) {
        g_warning("User data is not a valid progress bar");
    }

    int width, height;
    uint8_t *pixels;

    if (read_png_file("assets/input/image/image.png", &width, &height, &pixels) != 0) {
        return 1;
    }

    FILE *audio_file = fopen("assets/output/audio/output.wav", "wb");
    if (!audio_file) {
        fprintf(stderr, "Failed to open output WAV file.\n");
        free(pixels);
        return 1;
    }

    int num_pixels = width * height;
    write_wav_header(audio_file, num_pixels);

    // Store width and height after writing the WAV header
    fwrite(&width, sizeof(int), 1, audio_file);
    fwrite(&height, sizeof(int), 1, audio_file);

    if(g_selected_mode > 3){
        // Allocate a buffer for audio samples
        int16_t *samples = (int16_t *)malloc(num_pixels * sizeof(int16_t));
        if (samples == NULL) {
            fclose(audio_file);
            free(pixels);
            fprintf(stderr, "Error: Couldn't allocate memory for audio samples.\n");
            return 1;
        }

        // Convert each pixel to an audio sample with progress bar
        for (int i = 0; i < num_pixels; i++) {
            int r = pixels[4 * i];       // Red channel
            int g = pixels[4 * i + 1];   // Green channel
            int b = pixels[4 * i + 2];   // Blue channel
            int intensity = (r + g + b) / 3; // Grayscale intensity

            samples[i] = (int16_t)((intensity - 128) * 256); // Map intensity 0-255 to signed 16-bit audio

            // Update progress bar
            if (i % (num_pixels / 50) == 0 || i == num_pixels - 1) { // Update progress bar every 2% of the total
                float progress = (float)i / num_pixels;
                update_progress_bar(progress_bar, user_data, progress); // Use the GTK progress bar function
            }
        }

        // Write all samples to the WAV file at once
        generate_audio_samples(audio_file, samples, num_pixels);

        fclose(audio_file);
        free(pixels);
        free(samples);

        update_progress_bar(progress_bar, user_data, 1.0); // Final update to 100%
        printf("\nConversion to audio completed successfully!\n");

        set_text(builder);

        return 0;
    }else{
        // Initialize data structure
        Node *head = NULL, 
            *stack = NULL, 
            *queue_front = NULL, 
            *queue_rear = NULL;

        printf("Start --\n");

        for (int i = 0; i < width * height; i++) {
            int r = pixels[4 * i];       
            int g = pixels[4 * i + 1];
            int b = pixels[4 * i + 2];
            int intensity = (r + g + b) / 3;

            int16_t sample = (int16_t)((intensity - 128) * 256);

            if (g_selected_mode == 1) { 
                append_to_list(&head, sample);  // Linked List
            } else if (g_selected_mode == 2) {
                push_to_stack(&stack, sample); // Stack
            } else if (g_selected_mode == 3) {
                enqueue_to_queue(&queue_rear, &queue_front, sample); // Queue
            }

            // Update progress bar every 2% or at the last iteration
            if (i % (width * height / 50) == 0 || i == width * height - 1) {
                float progress = (float)(i + 1) / (width * height); // Progress as a fraction (0.0 - 1.0)
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), progress);

                // Process pending GTK events to update the UI
                while (gtk_events_pending()) {
                    gtk_main_iteration();
                }

                printf("%f", progress);
            }
        }

        // Write data to file from the chosen structure
        if (g_selected_mode == 1) {
            write_samples_from_structure(audio_file, head, g_selected_mode);  // Linked List
        } else if (g_selected_mode == 2) {
            write_samples_from_structure(audio_file, stack, g_selected_mode); // Stack
        } else if (g_selected_mode == 3) {
            write_samples_from_structure(audio_file, queue_front, g_selected_mode); // Queue
        }

        fclose(audio_file);
        free(pixels);

        update_progress_bar(progress_bar, user_data, 1.0); // Final update to 100%
        printf("Conversion to audio completed successfully using mode %d!\n", g_selected_mode);

        set_text(builder);

        return 0;
    }
}

// =========================================================================================================== wav - img
int main_audio_to_image(GtkProgressBar *progress_bar, gpointer user_data) {
    FILE *audio_file = fopen("assets/input/audio/input.wav", "rb");
    if (!audio_file) {
        fprintf(stderr, "Failed to open input WAV file.\n");
        return 1;
    }

    WavHeader header;
    read_wav_header(audio_file, &header);

    int num_samples = header.data_size / sizeof(int16_t);
    int width = 0, height = 0;

    // Read width and height from the end of the WAV file
    fread(&width, sizeof(int), 1, audio_file);
    fread(&height, sizeof(int), 1, audio_file);

    // Allocate memory for audio samples and pixels
    int16_t *samples = (int16_t *)malloc(num_samples * sizeof(int16_t));
    uint8_t *pixels = (uint8_t *)malloc(width * height * sizeof(uint8_t));
    if (samples == NULL || pixels == NULL) {
        fclose(audio_file);
        free(samples);
        free(pixels);
        fprintf(stderr, "Error: Couldn't allocate memory for samples or pixels.\n");
        return 1;
    }

    // Read samples from the WAV file
    fread(samples, sizeof(int16_t), num_samples, audio_file);
    fclose(audio_file);

    // Convert audio samples back to grayscale intensities
    for (int i = 0; i < num_samples; i++) {
        // Map signed 16-bit audio sample back to grayscale intensity
        int intensity = (samples[i] / 256) + 128; // Revert mapping
        // Clamp the intensity to [0, 255]
        if (intensity < 0) intensity = 0;
        if (intensity > 255) intensity = 255;
        pixels[i] = (uint8_t)intensity;
    }

    // Write the grayscale image to a PNG file
    write_png_file("assets/output/image/output.png", width, height, pixels);

    free(samples);
    free(pixels);

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 1.0);
    printf("Conversion back to image completed successfully!\n");
}

// ===========================================================================================================
// rist_wav_page
static void riset_wav(GtkBuilder *builder){
    // Get the file chooser button by its ID from the builder
    GtkWidget *file_chooser_button = GTK_WIDGET(gtk_builder_get_object(builder, "input_img_input"));

    if (file_chooser_button) {
        // Cast to GtkFileChooser and clear selection
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(file_chooser_button);
        gtk_file_chooser_unselect_all(chooser);

        printf("File selection cleared.\n");
    } else {
        printf("Error: Could not find file chooser button with ID 'input_img_input'.\n");
    }

    gtk_progress_bar_set_fraction(progress_bar, 0.0); // Update progress to 0

    // Get the error label (img)
    GtkLabel *error_label_img = GTK_LABEL(gtk_builder_get_object(builder, "err_img_input"));
    // Hide the error label on startups
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(error_label_img));

    // Get the image input progress box and hide
    GtkLabel *progress_box = GTK_LABEL(gtk_builder_get_object(builder, "conversion_img_progress_box"));
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(progress_box));

    // Get the output audio box and hide
    GtkLabel *output_box_wav = GTK_LABEL(gtk_builder_get_object(builder, "output_audio"));
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(output_box_wav));

    // delet files
    // File paths to delete
    const char *image_file = "assets/input/image/image.png";
    const char *audio_file = "assets/output/audio/output.wav";

    // Attempt to delete the image file
    if (remove(image_file) == 0) {
        printf("Successfully deleted file: %s\n", image_file);
    } else {
        perror("Error deleting image file");
    }

    // Attempt to delete the audio file
    if (remove(audio_file) == 0) {
        printf("Successfully deleted file: %s\n", audio_file);
    } else {
        perror("Error deleting audio file");
    }

    // Get the save_audio label (wav)
    GtkLabel *save_img_msg = GTK_LABEL(gtk_builder_get_object(builder, "saved_wav"));

    // Show error message with colored text
        gtk_widget_show(GTK_WIDGET(save_img_msg));
        char *file_error_message = g_strdup_printf("<span foreground=\"#007a0e\">!! .. File saved successfully .. !!</span>");
        gtk_label_set_markup(save_img_msg, file_error_message);
        g_free(file_error_message);

    // Hide the error label on startups
    g_timeout_add_seconds(5, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(save_img_msg));
}


// rist_img_page
static void riset_img(GtkBuilder *builder){
    // Get the file chooser button by its ID from the builder
    GtkWidget *file_chooser_button = GTK_WIDGET(gtk_builder_get_object(builder, "input_wav_input"));

    if (file_chooser_button) {
        // Cast to GtkFileChooser and clear selection
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(file_chooser_button);
        gtk_file_chooser_unselect_all(chooser);

        printf("File selection cleared.\n");
    } else {
        printf("Error: Could not find file chooser button with ID 'input_img_input'.\n");
    }

    gtk_progress_bar_set_fraction(progress_bar, 0.0); // Update progress to 0

    // Get the error label (img)
    GtkLabel *error_label_img = GTK_LABEL(gtk_builder_get_object(builder, "err_wav_input"));
    // Hide the error label on startups
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(error_label_img));

    // Get the image input progress box and hide
    GtkLabel *progress_box = GTK_LABEL(gtk_builder_get_object(builder, "conversion_wav_progress_box"));
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(progress_box));

    // Get the output audio box and hide
    GtkLabel *output_box_wav = GTK_LABEL(gtk_builder_get_object(builder, "output_image"));
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(output_box_wav));

    // delet files
    // File paths to delete
    const char *image_file = "assets/output/audio/output.png";
    const char *audio_file = "assets/input/audio/input.wav";

    // Attempt to delete the image file
    if (remove(image_file) == 0) {
        printf("Successfully deleted file: %s\n", image_file);
    } else {
        perror("Error deleting image file");
    }

    // Attempt to delete the audio file
    if (remove(audio_file) == 0) {
        printf("Successfully deleted file: %s\n", audio_file);
    } else {
        perror("Error deleting audio file");
    }

    // Get the save_audio label (wav)
    GtkLabel *save_img_msg = GTK_LABEL(gtk_builder_get_object(builder, "saved_img"));

    // Show error message with colored text
        gtk_widget_show(GTK_WIDGET(save_img_msg));
        char *file_error_message = g_strdup_printf("<span foreground=\"#007a0e\">!! .. File saved successfully .. !!</span>");
        gtk_label_set_markup(save_img_msg, file_error_message);
        g_free(file_error_message);

    // Hide the error label on startups
    g_timeout_add_seconds(5, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(save_img_msg));
}

// ------------------------------------------------------------------------------------------------------ Save Wav
// Callback function for the Save Audio button
static void on_save_button_wav_clicked(GtkButton *button, GtkBuilder *builder) {
    GtkWidget *dialog;
    GtkFileChooser *chooser;

    // Create a file chooser dialog
    dialog = gtk_file_chooser_dialog_new("Save Image",
                                         NULL, // Parent window, set this to your main window if needed
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Save", GTK_RESPONSE_ACCEPT,
                                         NULL);

    // Set the dialog to confirm on pressing 'Save'
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    // Run the dialog and get the response
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;
        chooser = GTK_FILE_CHOOSER(dialog);
        
        // Get the selected filename
        filename = gtk_file_chooser_get_filename(chooser);

        // Append .wav if not already present
        if (strstr(filename, ".wav") == NULL) {
            char *new_filename = g_strdup_printf("%s.wav", filename);
            g_free(filename); // Free the original filename
            filename = new_filename; // Use the new filename with .wav appended
        }
        
        // Now you can save your audio file to the selected path
        const char *source_path = "assets/output/audio/output.wav";

        // Check if source file exists before copying
        GFile *source_file = g_file_new_for_path(source_path);
        GFile *dest_file = g_file_new_for_path(filename);
        
        if (g_file_query_exists(source_file, NULL)) {
            // Copy the source audio file to the chosen filename
            GError *error = NULL;
            if (g_file_copy(source_file,
                            dest_file,
                            G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error)) {
                g_print("File saved successfully to: %s\n", filename);
            } else {
                // Print the error if the copy fails
                g_print("Failed to save file 'wav': %s\n", error->message);
                g_error_free(error);
            }
        } else {
            g_print("Source file 'wav' does not exist: %s\n", source_path);
        }

        g_free(filename); // Free the filename string
        riset_wav(builder);
    }

    gtk_widget_destroy(dialog); // Close the dialog
}

// ------------------------------------------------------------------------------------------------------ Save Img
// Callback function for the Save Audio button
static void on_save_button_img_clicked(GtkButton *button, GtkBuilder *builder) {
    GtkWidget *dialog;
    GtkFileChooser *chooser;

    // Create a file chooser dialog
    dialog = gtk_file_chooser_dialog_new("Save Audio",
                                         NULL, // Parent window, set this to your main window if needed
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Save", GTK_RESPONSE_ACCEPT,
                                         NULL);

    // Set the dialog to confirm on pressing 'Save'
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    // Run the dialog and get the response
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;
        chooser = GTK_FILE_CHOOSER(dialog);
        
        // Get the selected filename
        filename = gtk_file_chooser_get_filename(chooser);

        // Append .png if not already present
        if (strstr(filename, ".png") == NULL) {
            char *new_filename = g_strdup_printf("%s.png", filename);
            g_free(filename); // Free the original filename
            filename = new_filename; // Use the new filename with .wav appended
        }
        
        // Now you can save your audio file to the selected path
        const char *source_path = "assets/output/image/output.png";

        // Check if source file exists before copying
        GFile *source_file = g_file_new_for_path(source_path);
        GFile *dest_file = g_file_new_for_path(filename);
        
        if (g_file_query_exists(source_file, NULL)) {
            // Copy the source audio file to the chosen filename
            GError *error = NULL;
            if (g_file_copy(source_file,
                            dest_file,
                            G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error)) {
                g_print("File saved successfully to: %s\n", filename);
            } else {
                // Print the error if the copy fails
                g_print("Failed to save file 'img': %s\n", error->message);
                g_error_free(error);
            }
        } else {
            g_print("Source file 'img' does not exist: %s\n", source_path);
        }

        g_free(filename); // Free the filename string
        riset_img(builder);
    }

    gtk_widget_destroy(dialog); // Close the dialog
}

// ===========================================================================================================

// inmage to audio convertion button clicked
void on_conversion_button_clicked_img_wav(GtkWidget *widget, gpointer user_data) {
    const char *file_path = "assets/input/image/image.png";

    // Attempt to open the file in read mode
    FILE *file = fopen(file_path, "r");

    if (file) {
        // File exists
        printf("File exists: %s\n", file_path);
        fclose(file); // Close the file

        Status_img_wav *progress = (Status_img_wav *)user_data; // Cast user_data to Progress struct

        char *alert_converting = g_strdup_printf("<span foreground=\"#c7bd02\">Converting...</span>");
        gtk_label_set_markup(progress->con_progress_text, alert_converting);
        g_free(alert_converting);

        // Show the label (make it visible)
        gtk_widget_show(GTK_WIDGET(progress->conversion_label));

        // Ensure the progress bar is valid before updating it
        if (GTK_IS_PROGRESS_BAR(progress->progress_bar_img)) {
            // You can update the progress fraction here if needed
            gtk_progress_bar_set_fraction(progress->progress_bar_img, 0.0); // Example update

            // Optionally, update the label to show that the conversion has started
            gtk_label_set_text(GTK_LABEL(progress->conversion_label), "Conversion started...");

            // Call the img - wav conversion function
            g_print("Conversion started...\n");
            main_image_to_audio(progress->progress_bar_img, progress->conversion_label, progress->builder);
        } else {
            g_warning("Invalid progress bar or label");
        }

        // Show success message with colored text
        char *success_message = g_strdup_printf("<span foreground=\"#007a0e\">Conversion to audio completed successfully!</span>");
        gtk_label_set_markup(progress->con_progress_text, success_message);
        g_free(success_message);

        // Show the output
        gtk_widget_show(GTK_WIDGET(progress->output_box_audio));
    } else {
        // File does not exist
        printf("File does not exist: %s\n", file_path);

        Status_img_wav *progress = (Status_img_wav *)user_data; // Cast user_data to Progress struct

        // Show success message with colored text
        char *success_message = g_strdup_printf("<span foreground=\"#f51818\">Please select an image file!</span>");
        gtk_label_set_markup(progress->con_progress_text, success_message);
        g_free(success_message);
    }
}

// audio to image convertion button clicked
void on_conversion_button_clicked_wav_img(GtkWidget *widget, gpointer user_data) {
    Status_wav_img *progress = (Status_wav_img *)user_data; // Cast user_data to Progress struct

    char *alert_converting = g_strdup_printf("<span foreground=\"#c7bd02\">Converting...</span>");
    gtk_label_set_markup(progress->con_progress_text, alert_converting);
    g_free(alert_converting);

    // Show the label (make it visible)
    gtk_widget_show(GTK_WIDGET(progress->conversion_label));

    // Ensure the progress bar is valid before updating it
    if (GTK_IS_PROGRESS_BAR(progress->progress_bar_img)) {
        // You can update the progress fraction here if needed
        gtk_progress_bar_set_fraction(progress->progress_bar_img, 0.0); // Example update

        // Optionally, update the label to show that the conversion has started
        gtk_label_set_text(GTK_LABEL(progress->conversion_label), "Conversion started...");

        // Call the wav - img conversion function
        g_print("Conversion started...\n");
        main_audio_to_image(progress->progress_bar_img, progress->conversion_label);
    } else {
        g_warning("Invalid progress bar or label");
    }

    // Show success message with colored text
    char *success_message = g_strdup_printf("<span foreground=\"#007a0e\">Conversion to audio completed successfully!</span>");
    gtk_label_set_markup(progress->con_progress_text, success_message);
    g_free(success_message);

    // Show the output
    gtk_widget_show(GTK_WIDGET(progress->output_box_audio));
}

int main(int argc, char *argv[]) {
    // Initialize GTK
    gtk_init(&argc, &argv);

    // Create a builder and load the Glade XML file
    GtkBuilder *builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, "assets/ui/image_to_audio.glade", NULL);

    // Get the main window and set its properties
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "window_main"));
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Get the error label (img)
    GtkLabel *error_label_img = GTK_LABEL(gtk_builder_get_object(builder, "err_img_input"));
    // Hide the error label on startups
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(error_label_img));

    // Get the save_audio label (wav) and hide
    GtkLabel *save_img_msg = GTK_LABEL(gtk_builder_get_object(builder, "saved_wav"));
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(save_img_msg));

    // Get the save_audio label (wav) and hide
    GtkLabel *save_wav_msg = GTK_LABEL(gtk_builder_get_object(builder, "saved_img"));
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(save_wav_msg));

    // Get the error label (wav)
    GtkLabel *error_label_wav = GTK_LABEL(gtk_builder_get_object(builder, "err_wav_input"));
    // Hide the error label on startups
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(error_label_wav));

    // Get the image input progress box and hide
    GtkLabel *progress_box = GTK_LABEL(gtk_builder_get_object(builder, "conversion_img_progress_box"));
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(progress_box));

    // Get the image input progress box and hide
    GtkLabel *progress_box_wav = GTK_LABEL(gtk_builder_get_object(builder, "conversion_wav_progress_box"));
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(progress_box_wav));

    // Get the output audio box and hide
    GtkLabel *output_box_wav = GTK_LABEL(gtk_builder_get_object(builder, "output_audio"));
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(output_box_wav));

    // Get the output audio box and hide
    GtkLabel *output_box_img = GTK_LABEL(gtk_builder_get_object(builder, "output_image"));
    g_timeout_add_seconds(0, (GSourceFunc)gtk_widget_hide, GTK_WIDGET(output_box_img));

    // select samlerate
    GtkComboBoxText *samplerate_combo_sample = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "conversion_samplr_input"));
    GtkLabel *sample_rate_label = GTK_LABEL(gtk_builder_get_object(builder, "conversion_sample_name"));

    // select Mode
    GtkComboBoxText *samplerate_combo_mode = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "conversion_samplr_mode"));
    GtkLabel *mode_label = GTK_LABEL(gtk_builder_get_object(builder, "conversion_mode_name"));


    // Initialize AppData structure
    AppData app_data = {
        .error_label = error_label_img,
        .samplerate_combo_sample = samplerate_combo_sample,
        .samplerate_combo_mode = samplerate_combo_mode
    };

    // -----------------------------------------------------------------
    // Get the file chooser button and set a filter for PNG files only --1 (img)
    GtkFileChooser *file_chooser_img = GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "input_img_input"));
    GtkFileFilter *filter_img = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter_img, "*.png");
    gtk_file_chooser_set_filter(file_chooser_img, filter_img);

    // Get the conversion button using its ID from the Glade file
    GtkWidget *conversion_button_img = GTK_WIDGET(gtk_builder_get_object(builder, "conversion_button_img_wav"));

    // -----------------------------------------------------------------
    // Get the file chooser button and set a filter for WAV files only --2 (wav)
    GtkFileChooser *file_chooser_wav = GTK_FILE_CHOOSER(gtk_builder_get_object(builder, "input_wav_input"));
    GtkFileFilter *filter_wav = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter_wav, "*.wav");
    gtk_file_chooser_set_filter(file_chooser_wav, filter_wav);

    // Get the conversion button using its ID from the Glade file
    GtkWidget *conversion_button_wav = GTK_WIDGET(gtk_builder_get_object(builder, "conversion_button_wav_img"));

    Status_img_wav statusImageWav;

    // Get progress bar and label from the builder img - wav
    statusImageWav.progress_bar_img = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "progress_bar_img"));        // Progress bar
    statusImageWav.conversion_label = GTK_LABEL(gtk_builder_get_object(builder, "conversion_img_progress_box"));    // Progress box
    statusImageWav.con_progress_text = GTK_LABEL(gtk_builder_get_object(builder, "lebel_img_status"));              // Covert status
    statusImageWav.output_box_audio = GTK_LABEL(gtk_builder_get_object(builder, "output_audio"));                   // Output box
    statusImageWav.builder = builder;

    // Connect signal for the conversion button - image to audio -
    g_signal_connect(conversion_button_img, "clicked", G_CALLBACK(on_conversion_button_clicked_img_wav), &statusImageWav);

    Status_wav_img statusWavImg;

    // Get progress bar and label from the builder wav - img
    statusWavImg.progress_bar_img = GTK_PROGRESS_BAR(gtk_builder_get_object(builder, "progress_bar_wav"));        // Progress bar
    statusWavImg.conversion_label = GTK_LABEL(gtk_builder_get_object(builder, "conversion_wav_progress_box"));    // Progress box
    statusWavImg.con_progress_text = GTK_LABEL(gtk_builder_get_object(builder, "lebel_wav_status"));              // Covert status
    statusWavImg.output_box_audio = GTK_LABEL(gtk_builder_get_object(builder, "output_image"));                   // Output box

    // Connect signal for the conversion button - audio to image -
    g_signal_connect(conversion_button_wav, "clicked", G_CALLBACK(on_conversion_button_clicked_wav_img), &statusWavImg);

    // Get the save button by ID and connect the clicked signal -- wav
    GtkButton *save_button_wav = GTK_BUTTON(gtk_builder_get_object(builder, "save_wav_button"));
    g_signal_connect(save_button_wav, "clicked", G_CALLBACK(on_save_button_wav_clicked), builder);


    // Get the save button by ID and connect the clicked signal -- img
    GtkButton *save_button_img = GTK_BUTTON(gtk_builder_get_object(builder, "save_img_button"));
    g_signal_connect(save_button_img, "clicked", G_CALLBACK(on_save_button_img_clicked), builder);
    
    // Connect the file chooser to the file selection callback
    g_signal_connect(file_chooser_img, "file-set", G_CALLBACK(on_file_selected_img), error_label_img);

    // In any change selectior
    g_signal_connect(samplerate_combo_sample, "changed", G_CALLBACK(update_sample_rate_label), sample_rate_label);
    g_signal_connect(samplerate_combo_mode, "changed", G_CALLBACK(update_mode_label), mode_label);

    // Connect the file chooser to the file selection callback
    g_signal_connect(file_chooser_wav, "file-set", G_CALLBACK(on_file_selected_wav), error_label_wav);

    // Show the main window
    gtk_widget_show_all(window);

    // Start the GTK main loop
    gtk_main();

    return 0;

}

// ===========================================================================================================


// for Linux            -- gcc -o Wave2Image main.c -lpng -lm `pkg-config --cflags --libs gtk+-3.0`
// for static_linking   -- 

/*
    ++ Final image to sudio
*/