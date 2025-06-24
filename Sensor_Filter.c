#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_READINGS 10000
#define MAX_SENSORS 10

// Simple structure to hold readings - just arrays of doubles
typedef struct {
    double sensor_data[MAX_SENSORS][MAX_READINGS];  // [sensor_id][reading_index]
    int reading_counts[MAX_SENSORS];                // How many readings per sensor
    int max_sensor_id;                              // Highest sensor number found
    int total_readings;                             // Total across all sensors
} SensorArrays;

// Parse file and return simple arrays
SensorArrays* parse_sensor_file(const char *filename) {
    FILE *file = fopen(filename, "r"); // read file
        if (!file) {                                                // check if file will open or crash
            fprintf(stderr, "Error opening file: %s\n", filename);
            return NULL;
        }
    
    SensorArrays *arrays = calloc(1, sizeof(SensorArrays)); // create space to hold data
    char line[256]; // space per line to read numbers 
    
    while (fgets(line, sizeof(line), file)) { // while we can still read sensor data
        int sensor_num;
        double value;
        
        // format to read : I (timestamp) AJSR04M: Sensor X: value cm
        if (sscanf(line, "%*s %*s %*s Sensor %d: %lf cm", &sensor_num, &value) == 2) {
            // Convert to 0-based index (Sensor 1 -> index 0, Sensor 2 -> index 1)
            int sensor_idx = sensor_num - 1; // because indice 0 is sensor 1
            
            if (sensor_idx >= 0 && sensor_idx < MAX_SENSORS) { // so we dont max out sensors
                int count = arrays->reading_counts[sensor_idx]; // get number of readings for this sensor so i can store next value in the next spot
                if (count < MAX_READINGS) { // dont overflow readings array
                    arrays->sensor_data[sensor_idx][count] = value; // store value on the right spot
                    arrays->reading_counts[sensor_idx]++;           //increment placement and total readings count
                    arrays->total_readings++;
                    
                    if (sensor_num > arrays->max_sensor_id) { // keep track of the highest sensor number, modular
                        arrays->max_sensor_id = sensor_num;
                    }
                }
            }
        }
    }
    
    fclose(file); // close the file
    return arrays;
}

// Get pointer to specific sensor's data array (for easy graphing)
double* get_sensor_array(SensorArrays *arrays, int sensor_number) {
    int sensor_idx = sensor_number - 1;
    if (sensor_idx >= 0 && sensor_idx < MAX_SENSORS) {
        return arrays->sensor_data[sensor_idx];
    }
    return NULL;
}

// Get count of readings for a sensor
int get_sensor_count(SensorArrays *arrays, int sensor_number) {
    int sensor_idx = sensor_number - 1;
    if (sensor_idx >= 0 && sensor_idx < MAX_SENSORS) {
        return arrays->reading_counts[sensor_idx];
    }
    return 0;
}

/* Print summary
void print_summary(SensorArrays *arrays) {
    printf("Found %d sensors with %d total readings:\n", 
           arrays->max_sensor_id, arrays->total_readings);
    
    for (int i = 0; i < arrays->max_sensor_id; i++) {
        if (arrays->reading_counts[i] > 0) {
            printf("Sensor %d: %d readings (%.2f to %.2f)\n", 
                   i + 1, 
                   arrays->reading_counts[i],
                   arrays->sensor_data[i][0],
                   arrays->sensor_data[i][arrays->reading_counts[i] - 1]);
        }
    }
}*/



// median filter function, multiple filters-- matlab  

/ Helper function to find median of an array
double find_median(double *arr, int size) {
    // Create a copy to sort (don't modify original)
    double *temp = malloc(size * sizeof(double));
    for (int i = 0; i < size; i++) {
        temp[i] = arr[i];
    }
    
    // Simple bubble sort (fine for small window sizes)
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (temp[j] > temp[j + 1]) {
                double swap = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = swap;
            }
        }
    }
    
    double median;
    if (size % 2 == 0) {
        // Even number of elements - average the two middle ones
        median = (temp[size/2 - 1] + temp[size/2]) / 2.0;
    } else {
        // Odd number of elements - take the middle one
        median = temp[size/2];
    }
    
    free(temp);
    return median;
}

// Apply median filter to a sensor's data
void apply_median_filter(SensorArrays *arrays, int sensor_number, int window_size, int iterations) {
    int sensor_idx = sensor_number - 1;
    
    // Validate inputs
    if (sensor_idx < 0 || sensor_idx >= MAX_SENSORS) {
        printf("Error: Invalid sensor number %d\n", sensor_number);
        return;
    }
    
    if (arrays->reading_counts[sensor_idx] == 0) {
        printf("Error: No data for sensor %d\n", sensor_number);
        return;
    }
    
    if (window_size < 3 || window_size % 2 == 0) {
        printf("Error: Window size must be odd and >= 3. Using window_size = 3\n");
        window_size = 3;
    }
    
    int data_count = arrays->reading_counts[sensor_idx];
    if (window_size > data_count) {
        printf("Error: Window size (%d) larger than data count (%d)\n", window_size, data_count);
        return;
    }
    
    printf("Applying median filter to Sensor %d: window=%d, iterations=%d\n", 
           sensor_number, window_size, iterations);
    
    // Apply filter multiple times
    for (int iter = 0; iter < iterations; iter++) {
        // Create temporary array to store filtered results
        double *filtered = malloc(data_count * sizeof(double));
        
        int half_window = window_size / 2;
        
        // Process each data point
        for (int i = 0; i < data_count; i++) {
            if (i < half_window || i >= data_count - half_window) {
                // Edge cases: not enough neighbors, keep original value
                filtered[i] = arrays->sensor_data[sensor_idx][i];
            } else {
                // Normal case: apply median filter
                double window[window_size];
                for (int j = 0; j < window_size; j++) {
                    window[j] = arrays->sensor_data[sensor_idx][i - half_window + j];
                }
                filtered[i] = find_median(window, window_size);
            }
        }
        
        // Copy filtered data back to original array
        for (int i = 0; i < data_count; i++) {
            arrays->sensor_data[sensor_idx][i] = filtered[i];
        }
        
        free(filtered);
        
        printf("  Iteration %d complete\n", iter + 1);
    }
    
    printf("Median filtering complete for Sensor %d\n", sensor_number);
}


// graph data functions

void graph_sensor_simple(SensorArrays *arrays, int sensor_number, int window_size, int iterations) {
    int sensor_idx = sensor_number - 1;
    
    // Validate inputs
    if (sensor_idx < 0 || sensor_idx >= MAX_SENSORS) {
        printf("Error: Invalid sensor number %d\n", sensor_number);
        return;
    }
    
    if (arrays->reading_counts[sensor_idx] == 0) {
        printf("Error: No data for sensor %d\n", sensor_number);
        return;
    }
    
    int data_count = arrays->reading_counts[sensor_idx];
    
    // Create a copy of original data before filtering
    double *original_data = malloc(data_count * sizeof(double));
    for (int i = 0; i < data_count; i++) {
        original_data[i] = arrays->sensor_data[sensor_idx][i];
    }
    
    // Apply median filter
    apply_median_filter(arrays, sensor_number, window_size, iterations);
    
    // Save data to files for external plotting
    char orig_filename[256], filt_filename[256];
    snprintf(orig_filename, sizeof(orig_filename), "sensor_%d_original.dat", sensor_number);
    snprintf(filt_filename, sizeof(filt_filename), "sensor_%d_filtered.dat", sensor_number);
    
    FILE *orig_file = fopen(orig_filename, "w");
    FILE *filt_file = fopen(filt_filename, "w");
    
    if (!orig_file || !filt_file) {
        printf("Error: Could not create data files\n");
        free(original_data);
        return;
    }
    
    fprintf(orig_file, "# Time(index) Original_Value(cm)\n");
    fprintf(filt_file, "# Time(index) Filtered_Value(cm)\n");
    
    for (int i = 0; i < data_count; i++) {
        fprintf(orig_file, "%d %.3f\n", i, original_data[i]);
        fprintf(filt_file, "%d %.3f\n", i, arrays->sensor_data[sensor_idx][i]);
    }
    
    fclose(orig_file);
    fclose(filt_file);
    
    printf("Data files created:\n");
    printf("  Original: %s\n", orig_filename);
    printf("  Filtered: %s\n", filt_filename);
    
    // Create gnuplot script
    char script_filename[256];
    snprintf(script_filename, sizeof(script_filename), "plot_sensor_%d.gp", sensor_number);
    
    FILE *script = fopen(script_filename, "w");
    if (script) {
        fprintf(script, "set title 'Sensor %d Data Comparison (Window=%d, Iterations=%d)'\n", 
                sensor_number, window_size, iterations);
        fprintf(script, "set xlabel 'Reading Index (Time)'\n");
        fprintf(script, "set ylabel 'Distance (cm)'\n");
        fprintf(script, "set grid\n");
        fprintf(script, "set key top right\n");
        
        char png_filename[256];
        snprintf(png_filename, sizeof(png_filename), "sensor_%d_comparison.png", sensor_number);
        
        fprintf(script, "set terminal png size 1200,800\n");
        fprintf(script, "set output '%s'\n", png_filename);
        fprintf(script, "plot '%s' using 1:2 with lines lw 2 title 'Original Data', \\\n", orig_filename);
        fprintf(script, "     '%s' using 1:2 with lines lw 2 title 'Filtered Data'\n", filt_filename);
        
        fclose(script);
        
        // Try to execute gnuplot
        char command[512];
        snprintf(command, sizeof(command), "gnuplot %s", script_filename);
        
        if (system(command) == 0) {
            printf("Graph saved as: %s\n", png_filename);
        } else {
            printf("Gnuplot script created: %s\n", script_filename);
            printf("Run 'gnuplot %s' to generate the graph\n", script_filename);
        }
    }
    
    free(original_data);
}


