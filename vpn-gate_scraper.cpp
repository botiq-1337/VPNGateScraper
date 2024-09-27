#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <chrono>
#include <algorithm>

#include <curl/curl.h>
#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>

#define VPNGATE_SCRAPER "VPNGate OpenVPN Scraper"
#define SEPARATOR_STR "------------------------------\n"

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static int curl_get(std::string url, std::string *buffer)
{
	CURL *curl;

	CURLcode res;
  	curl_global_init(CURL_GLOBAL_DEFAULT);
 
  	curl = curl_easy_init();
  	if (curl) {
    	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
		res = curl_easy_perform(curl);
	    if (res != CURLE_OK) {
	    	return -1;
	    }
	    curl_easy_cleanup(curl);
	} else {
		return -1;
	}
 
  	curl_global_cleanup();

  	return 0;
}

static std::string decode_base64(std::string input)
{
  	using namespace boost::archive::iterators;
  	typedef transform_width<binary_from_base64<remove_whitespace<std::string::const_iterator> >, 8, 6> ItBinaryT;

  	try
  	{
    	size_t num_pad_chars((4 - input.size() % 4) % 4);
    	input.append(num_pad_chars, '=');

    	size_t pad_chars(std::count(input.begin(), input.end(), '='));
    	std::replace(input.begin(), input.end(), '=', 'A');
    	std::string output(ItBinaryT(input.begin()), ItBinaryT(input.end()));
    	output.erase(output.end() - pad_chars, output.end());
    	return output;
  	}
  	catch (std::exception const&)
  	{
    	return std::string("");
  	}
}

int main(int argc, char *argv[])
{
	std::cout << VPNGATE_SCRAPER << std::endl << SEPARATOR_STR;

	const std::vector<std::string> params(argv, argv + argc); 
	const std::string vpngate_url = "https://www.vpngate.net/api/iphone/";
	const std::string current_working_directory = std::filesystem::current_path();
	std::string dump_directory = current_working_directory + "/VPNDump-" + std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
	std::string buffer;

	std::cout << "Downloading to " << dump_directory << std::endl << SEPARATOR_STR;

  	if (curl_get(vpngate_url, &buffer) < 0) {
    	std::cout << "Request failed at : " << vpngate_url << std::endl;
    	return -1;
  	}

  	std::cout << "Finished downloading, now unpacking.." << std::endl << SEPARATOR_STR;

	/**
	* ##########################################################
	**/

  	std::filesystem::create_directory(dump_directory);

	std::vector<std::string> csv_lines;
	boost::split(csv_lines, buffer, boost::is_any_of("\n"));
	csv_lines.erase(csv_lines.begin(), csv_lines.begin() + 2);
	csv_lines.erase(csv_lines.end() - 2, csv_lines.end());

	std::for_each(csv_lines.begin(), csv_lines.end(), [&dump_directory](std::string &vpn_data){
		std::vector<std::string> vpn_infos;
		boost::split(vpn_infos, vpn_data, boost::is_any_of(","));

		std::string vpn_name = vpn_infos[0] + "_" + vpn_infos[1];
		std::string vpn_path = dump_directory + "/" + vpn_name + ".ovpn";
		std::cout << vpn_path << std::endl;

		std::ofstream vpn_file(vpn_path);
		vpn_file << decode_base64(vpn_infos[14]) << std::endl;
		vpn_file.close();
	});
	
	return 0;
}