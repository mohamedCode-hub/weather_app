#include "weather.h"
#include <iostream>
#include <iomanip> // for std::put_time
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"

std::atomic<bool> dataReady(false);

void fetchWeatherDataInBackground(const std::string& cityName, const std::string& apiKey, nlohmann::json& weatherData) {
    try {
        httplib::SSLClient cli("api.openweathermap.org");
        cli.set_follow_location(true);
        std::string path = "/data/2.5/weather?q=" + cityName + "&appid=" + apiKey + "&units=metric";

        if (auto res = cli.Get(path.c_str())) {
            if (res->status == 200) {
                try {
                    weatherData = nlohmann::json::parse(res->body);
                    dataReady.store(true);
                }
                catch (const std::exception& e) {
                    std::cerr << "Error parsing weather data: " << e.what() << std::endl;
                    dataReady.store(false);
                }
            }
            else {
                std::cerr << "Failed to fetch weather data. HTTP Status: " << res->status << std::endl;
                dataReady.store(false);
            }
        }
        else {
            std::cerr << "Connection failed" << std::endl;
            dataReady.store(false);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        dataReady.store(false);
    }
}
void fetchWeatherForecastInBackground(const std::string& cityName, const std::string& apiKey, nlohmann::json& forecastData) {
    try {
        httplib::SSLClient cli("api.openweathermap.org");
        cli.set_follow_location(true);
        std::string path = "/data/2.5/forecast?q=" + cityName + "&appid=" + apiKey + "&units=metric";

        if (auto res = cli.Get(path.c_str())) {
            if (res->status == 200) {
                try {
                    forecastData = nlohmann::json::parse(res->body);
                    dataReady.store(true);
                }
                catch (const std::exception& e) {
                    std::cerr << "Error parsing weather forecast data: " << e.what() << std::endl;
                    dataReady.store(false);
                }
            }
            else {
                std::cerr << "Failed to fetch weather forecast data. HTTP Status: " << res->status << std::endl;
                dataReady.store(false);
            }
        }
        else {
            std::cerr << "Connection failed" << std::endl;
            dataReady.store(false);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        dataReady.store(false);
    }
}

void displayWeatherData(const nlohmann::json& weatherData, const nlohmann::json& forecastData, bool showFahrenheit) {
    ImGui::Text("City: %s", weatherData["name"].get<std::string>().c_str());

    double temp = weatherData["main"]["temp"].get<double>();
    if (showFahrenheit) {
        temp = temp * 9 / 5 + 32;
        ImGui::Text("Temperature: %.2f°F", temp);
    }
    else {
        ImGui::Text("Temperature: %.2f°C", temp);
    }

    ImGui::Text("Humidity: %d%%", weatherData["main"]["humidity"].get<int>());
    ImGui::Text("Wind Speed: %.2f m/s", weatherData["wind"]["speed"].get<double>());
    ImGui::Text("Pressure: %d hPa", weatherData["main"]["pressure"].get<int>());
    ImGui::Text("Visibility: %d m", weatherData["visibility"].get<int>());
    ImGui::Text("Sunrise: %s", convertUnixToTime(weatherData["sys"]["sunrise"].get<int>()).c_str());
    ImGui::Text("Sunset: %s", convertUnixToTime(weatherData["sys"]["sunset"].get<int>()).c_str());

    // Additional data points
    if (weatherData.contains("main") && weatherData["main"].contains("feels_like")) {
        double feelsLike = weatherData["main"]["feels_like"].get<double>();
        if (showFahrenheit) {
            feelsLike = feelsLike * 9 / 5 + 32;
            ImGui::Text("Feels Like: %.2f°F", feelsLike);
        }
        else {
            ImGui::Text("Feels Like: %.2f°C", feelsLike);
        }
    }

    if (weatherData.contains("visibility")) {
        ImGui::Text("Visibility: %d meters", weatherData["visibility"].get<int>());
    }

    if (weatherData.contains("rain") && weatherData["rain"].contains("1h")) {
        ImGui::Text("Rainfall (Last 1h): %.2f mm", weatherData["rain"]["1h"].get<double>());
    }

    // Display forecast for the past 7 days (simulated)
    if (forecastData.is_object()) {
        ImGui::Separator();
        ImGui::Text("Past 7 Days Forecast:");

        // Get the current date
        auto now = std::chrono::system_clock::now();

        for (int i = 0; i < 7; ++i) {
            // Calculate the date for the past i-th day
            auto pastDate = now - std::chrono::hours(24 * i);
            std::time_t pastTime = std::chrono::system_clock::to_time_t(pastDate);
            std::tm pastTm;
            localtime_s(&pastTm, &pastTime);

            char dateBuffer[64];
            std::strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &pastTm);

            double forecastTemp = forecastData["list"][i]["main"]["temp"].get<double>();
            if (showFahrenheit) {
                forecastTemp = forecastTemp * 9 / 5 + 32;
                ImGui::Text("%s - %.2f°F", dateBuffer, forecastTemp);
            }
            else {
                ImGui::Text("%s - %.2f°C", dateBuffer, forecastTemp);
            }
        }
    }
}



std::string convertUnixToTime(int unixTime) {
    std::time_t t = unixTime;
    std::tm tm;
    localtime_s(&tm, &t); // Use localtime_s instead of localtime
    char buffer[80];
    strftime(buffer, 80, "%H:%M:%S", &tm);
    return std::string(buffer);
}