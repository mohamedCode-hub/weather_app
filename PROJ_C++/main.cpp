#include "GL/glew.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"
#include "weather.h"
#include <thread>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <windows.h>
#include "resource1.h"
#include <chrono>  // For getting the current time
#include <iomanip> // For formatting time

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

std::string cityName = "London"; // Default city
bool showFahrenheit = false; // Toggle between Celsius and Fahrenheit
std::string weatherCondition = "Clear"; // Default weather condition
std::string fetchTime = "";

GLuint cloudyTexture;
GLuint sunnyTexture;
GLuint rainyTexture;
GLuint snowyTexture;

// Cloud struct for moving clouds
struct Cloud {
    float x, y;
    float speed;
};

struct Particle {
    float x, y;
    float speedX, speedY;
    float lifetime;
};

std::vector<Cloud> clouds;
std::vector<Particle> particles;
std::vector<std::string> favoriteCities;

std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
    localtime_s(&localTime, &currentTime);  // Use localtime_s instead of localtime

    char timeBuffer[64];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return std::string(timeBuffer);
}

// Function to load a texture from resources
GLuint loadTextureFromResource(HMODULE hModule, int resourceID) {
    HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(resourceID), RT_RCDATA);
    if (!hRes) {
        std::cerr << "Failed to find resource: " << resourceID << " Error: " << GetLastError() << std::endl;
        return 0;
    }

    HGLOBAL hMem = LoadResource(hModule, hRes);
    if (!hMem) {
        std::cerr << "Failed to load resource: " << resourceID << " Error: " << GetLastError() << std::endl;
        return 0;
    }

    LPVOID lpResData = LockResource(hMem);
    DWORD dwSize = SizeofResource(hModule, hRes);

    int width, height, nrChannels;
    unsigned char* data = stbi_load_from_memory((unsigned char*)lpResData, dwSize, &width, &height, &nrChannels, 0);
    if (!data) {
        std::cerr << "Failed to load texture from memory for resource: " << resourceID << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    return textureID;
}

// Load textures for different weather conditions
void loadWeatherTextures(HMODULE hModule) {
    cloudyTexture = loadTextureFromResource(hModule, IDB_BITMAP1);
    sunnyTexture = loadTextureFromResource(hModule, IDB_BITMAP2);
    rainyTexture = loadTextureFromResource(hModule, IDB_BITMAP3);
    snowyTexture = loadTextureFromResource(hModule, IDB_BITMAP4);

    if (cloudyTexture == 0 || sunnyTexture == 0 || rainyTexture == 0) {
        std::cerr << "Failed to load one or more textures" << std::endl;
    }
}

// Initialize cloud positions and speeds
void initClouds(int numClouds) {
    clouds.clear();
    for (int i = 0; i < numClouds; ++i) {
        Cloud cloud;
        cloud.x = static_cast<float>(rand() % 200 - 100) / 100.0f; // Random x between -1.0 and 1.0
        cloud.y = static_cast<float>(rand() % 100) / 100.0f;       // Random y between 0.0 and 1.0
        cloud.speed = static_cast<float>(rand() % 50 + 10) / 10000.0f; // Random speed between 0.001 and 0.005
        clouds.push_back(cloud);
    }
}

// Initialize particles for rain or snow
void initParticles(int numParticles) {
    particles.clear();
    for (int i = 0; i < numParticles; ++i) {
        Particle particle;
        particle.x = static_cast<float>(rand() % 200 - 100) / 100.0f;
        particle.y = static_cast<float>(rand() % 100) / 100.0f + 1.0f;
        particle.speedX = static_cast<float>(rand() % 20 - 10) / 10000.0f;
        particle.speedY = static_cast<float>(rand() % 50 + 50) / 10000.0f;
        particle.lifetime = 1.0f;
        particles.push_back(particle);
    }
}

// Update particles for animation
void updateParticles(float deltaTime) {
    for (auto& particle : particles) {
        particle.x += particle.speedX * deltaTime;
        particle.y -= particle.speedY * deltaTime;
        particle.lifetime -= deltaTime;

        if (particle.y < -1.0f || particle.lifetime <= 0.0f) {
            particle.x = static_cast<float>(rand() % 200 - 100) / 100.0f;
            particle.y = 1.0f;
            particle.speedX = static_cast<float>(rand() % 20 - 10) / 10000.0f;
            particle.speedY = static_cast<float>(rand() % 50 + 50) / 10000.0f;
            particle.lifetime = 1.0f;
        }
    }
}

// Render the particles on screen
void renderParticles() {
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_POINTS);
    for (const auto& particle : particles) {
        glVertex2f(particle.x, particle.y);
    }
    glEnd();
}

std::vector<float> getTemperatureData() {
    return { 22.5f, 23.0f, 21.5f, 24.0f, 25.5f, 23.0f, 22.0f }; // Example temperatures over a week
}

// Render the temperature graph
void renderTemperatureGraph() {
    std::vector<float> temperatures = getTemperatureData();
    ImGui::Text("Temperature Trend (Past Week):");
    ImGui::PlotLines("", temperatures.data(), temperatures.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 80));
}

// Render favorite cities and allow selection
void renderFavoriteCities(nlohmann::json& weatherData) {
    ImGui::Text("Favorite Cities:");
    for (const auto& city : favoriteCities) {
        if (ImGui::Button(city.c_str())) {
            cityName = city;
            dataReady = false;
            fetchTime = getCurrentTime(); // Update fetch time
            std::thread t(fetchWeatherDataInBackground, cityName, "7cb136b5cad5f1a822d6ea35ecf209d9", std::ref(weatherData));
            t.detach();
        }
    }

    if (ImGui::Button("Add to Favorites")) {
        if (!cityName.empty() && std::find(favoriteCities.begin(), favoriteCities.end(), cityName) == favoriteCities.end()) {
            favoriteCities.push_back(cityName);
        }
    }
}

// Render the UI for the weather application
void renderUI(nlohmann::json& weatherData, nlohmann::json& forecastData) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.2f, 0.5f, 0.9f)); // Transparent, dark blue background

    ImGui::Begin("Weather Application", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowPos(ImVec2(50, 50));
    ImGui::SetWindowSize(ImVec2(450, 650));

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.6f, 1.0f), "Weather Forecast");
    ImGui::Separator();

    // Display Current Time
    ImGui::Text("Current Time: ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", getCurrentTime().c_str());

    // Display Fetch Time
    ImGui::Text("Last Fetch Time: ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "%s", fetchTime.c_str());

    ImGui::Separator();

    // City Input
    static char cityBuffer[128] = "";
    ImGui::InputTextWithHint("##city", "Enter City Name", cityBuffer, IM_ARRAYSIZE(cityBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Fetch")) {
        cityName = cityBuffer;
        dataReady = false;
        fetchTime = getCurrentTime(); // Update fetch time
        std::thread t(fetchWeatherDataInBackground, cityName, "7cb136b5cad5f1a822d6ea35ecf209d9", std::ref(weatherData));
        t.detach();

        // Also fetch the weather forecast
        std::thread forecastThread(fetchWeatherForecastInBackground, cityName, "7cb136b5cad5f1a822d6ea35ecf209d9", std::ref(forecastData));
        forecastThread.detach();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        dataReady = false;
        fetchTime = getCurrentTime(); // Update fetch time
        std::thread t(fetchWeatherDataInBackground, cityName, "7cb136b5cad5f1a822d6ea35ecf209d9", std::ref(weatherData));
        t.detach();

        // Also fetch the weather forecast
        std::thread forecastThread(fetchWeatherForecastInBackground, cityName, "7cb136b5cad5f1a822d6ea35ecf209d9", std::ref(forecastData));
        forecastThread.detach();
    }

    ImGui::Checkbox("Show in Fahrenheit", &showFahrenheit);
    renderFavoriteCities(weatherData);

    if (!weatherData.is_null() && dataReady) {
        weatherCondition = weatherData["weather"][0]["main"].get<std::string>();
        ImGui::Separator();
        ImGui::Text("Current Condition: ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", weatherCondition.c_str());

        double temp = weatherData["main"]["temp"].get<double>();
        ImGui::Text("Temperature: ");
        ImGui::SameLine();
        if (showFahrenheit) {
            temp = temp * 9 / 5 + 32;
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "%.2f°F", temp);
        }
        else {
            ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "%.2f°C", temp);
        }

        displayWeatherData(weatherData, forecastData, showFahrenheit);
        renderTemperatureGraph();
    }
    else {
        ImGui::Text("Fetching weather data...");
    }

    ImGui::End();
    ImGui::PopStyleVar(); // Balanced with PushStyleVar
    ImGui::PopStyleColor(); // Balanced with PushStyleColor
}

// Render the background based on the weather condition
void renderBackground() {
    GLuint textureID = 0;

    if (weatherCondition == "Clouds") {
        textureID = cloudyTexture;
    }
    else if (weatherCondition == "Clear") {
        textureID = sunnyTexture;
    }
    else if (weatherCondition == "Rain") {
        textureID = rainyTexture;
    }

    if (textureID) {
        glBindTexture(GL_TEXTURE_2D, textureID);

        glEnable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, 1.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }
    else {
        std::cerr << "Texture ID is 0, skipping render." << std::endl;
    }
}

// Main function to initialize and run the weather application
int main() {
    srand(static_cast<unsigned>(time(0))); // Seed random number generator
    initClouds(10); // Initialize clouds with random positions

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create a windowed mode window and its OpenGL context
    GLFWwindow* window = glfwCreateWindow(2000, 1000, "Weather Application", glfwGetPrimaryMonitor(), NULL); // Fullscreen mode
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    HMODULE hModule = GetModuleHandle(NULL);
    loadWeatherTextures(hModule); // Load textures for different weather conditions

    nlohmann::json weatherData;
    nlohmann::json forecastData;

    if (weatherCondition == "Rain" || weatherCondition == "Snow") {
        initParticles(100); // Initialize particles for rain or snow
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderBackground(); // Render the weather background

        if (weatherCondition == "Rain" || weatherCondition == "Snow") {
            updateParticles(0.016f); // Update particles with a 60 FPS assumption
            renderParticles(); // Render particles
        }

        renderUI(weatherData, forecastData);

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
