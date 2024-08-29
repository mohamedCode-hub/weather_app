#ifndef WEATHER_H
#define WEATHER_H

#include <string>
#include <atomic>
#include <json.hpp>
#include <imgui.h>

// Declare these variables as extern in the header
extern std::string cityName;
extern bool showFahrenheit;
extern std::string weatherCondition;
extern std::atomic<bool> dataReady;

void fetchWeatherDataInBackground(const std::string& cityName, const std::string& apiKey, nlohmann::json& weatherData);
void displayWeatherData(const nlohmann::json& weatherData, const nlohmann::json& forecastData, bool showFahrenheit);
void renderUI(nlohmann::json& weatherData);
void renderBackground();
void initClouds(int numClouds);
std::string convertUnixToTime(int unixTime);
void fetchWeatherForecastInBackground(const std::string& cityName, const std::string& apiKey, nlohmann::json& forecastData);

#endif // WEATHER_H
