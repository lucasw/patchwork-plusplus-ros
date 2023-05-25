#ifndef PATCHWORKPP_PARAMS
#define PATCHWORKPP_PARAMS

#include <regex>
#include <vector>
#include <stdexcept>
#include <ros/ros.h>
#include <dynamic_reconfigure/server.h>
#include <patchworkpp/PatchworkppConfig.h>

class ParamsNh {
public:
    ParamsNh() {
        auto f = boost::bind(&ParamsNh::reconfigure_callback, this, _1, _2);
        server_.setCallback(f);
        validate();
    }

    ParamsNh(const ros::NodeHandle& nh) {
        nh.param("verbose", verbose_, false);
        nh.param("sensor_height", sensor_height_, 1.723);
        nh.param("num_iter", num_iter_, 3);
        nh.param("num_lpr", num_lpr_, 20);
        nh.param("num_min_pts", num_min_pts_, 10);
        nh.param("th_seeds", th_seeds_, 0.4);
        nh.param("th_dist", th_dist_, 0.3);
        nh.param("th_seeds_v", th_seeds_v_, 0.4);
        nh.param("th_dist_v", th_dist_v_, 0.3);
        nh.param("max_r", max_range_, 80.0);
        nh.param("min_r", min_range_, 2.7);
        nh.param("uprightness_thr", uprightness_thr_, 0.5);
        nh.param("adaptive_seed_selection_margin", adaptive_seed_selection_margin_, -1.1);
        nh.param("RNR_ver_angle_thr", RNR_ver_angle_thr_, -15.0);
        nh.param("RNR_intensity_thr", RNR_intensity_thr_, 0.2);
        nh.param("max_flatness_storage", max_flatness_storage_, 1000);
        nh.param("max_elevation_storage", max_elevation_storage_, 1000);
        nh.param("enable_RNR", enable_RNR_, true);
        nh.param("enable_RVPF", enable_RVPF_, true);
        nh.param("enable_TGR", enable_TGR_, true);

        // CZM denotes 'Concentric Zone Model'. Please refer to our paper
        nh.getParam("czm/num_zones", czm.num_zones_);
        nh.getParam("czm/num_sectors_each_zone", czm.num_sectors_each_zone_);
        nh.getParam("czm/num_rings_each_zone", czm.num_rings_each_zone_);
        nh.getParam("czm/elevation_thresholds", czm.elevation_thr_);
        nh.getParam("czm/flatness_thresholds", czm.flatness_thr_);

        validate();

        num_rings_of_interest_ = czm.elevation_thr_.size();

        auto min_range_z2 = (7 * min_range_ + max_range_) / 8.0;
        auto min_range_z3 = (3 * min_range_ + max_range_) / 4.0;
        auto min_range_z4 = (min_range_ + max_range_) / 2.0;

        min_ranges_ = {min_range_, min_range_z2, min_range_z3, min_range_z4};
        ring_sizes_ = {(min_range_z2 - min_range_) / czm.num_rings_each_zone_.at(0),
                      (min_range_z3 - min_range_z2) / czm.num_rings_each_zone_.at(1),
                      (min_range_z4 - min_range_z3) / czm.num_rings_each_zone_.at(2),
                      (max_range_ - min_range_z4) / czm.num_rings_each_zone_.at(3)};
        sector_sizes_ = {2 * M_PI / czm.num_sectors_each_zone_.at(0), 2 * M_PI / czm.num_sectors_each_zone_.at(1),
                        2 * M_PI / czm.num_sectors_each_zone_.at(2),
                        2 * M_PI / czm.num_sectors_each_zone_.at(3)};

        initialized_ = true;
        
    }

    void print_params() const {
        ROS_INFO("Sensor Height: %f", sensor_height_);
        ROS_INFO("Num of Iteration: %d", num_iter_);
        ROS_INFO("Num of LPR: %d", num_lpr_);
        ROS_INFO("Num of min. points: %d", num_min_pts_);
        ROS_INFO("Seeds Threshold: %f", th_seeds_);
        ROS_INFO("Distance Threshold: %f", th_dist_);
        ROS_INFO("Max. range:: %f", max_range_);
        ROS_INFO("Min. range:: %f", min_range_);
        ROS_INFO("Normal vector threshold: %f", uprightness_thr_);
        ROS_INFO("adaptive_seed_selection_margin: %f", adaptive_seed_selection_margin_);
        ROS_INFO("Num. zones: %d", czm.num_zones_);
        ROS_INFO_STREAM((boost::format("Num. sectors: %d, %d, %d, %d") % czm.num_sectors_each_zone_[0] % czm.num_sectors_each_zone_[1] %
                 czm.num_sectors_each_zone_[2] %
                 czm.num_sectors_each_zone_[3]).str());
        ROS_INFO_STREAM((boost::format("Num. rings: %01d, %01d, %01d, %01d") % czm.num_rings_each_zone_[0] %
                 czm.num_rings_each_zone_[1] %
                 czm.num_rings_each_zone_[2] %
                 czm.num_rings_each_zone_[3]).str());
        ROS_INFO_STREAM((boost::format("elevation_thr_: %0.4f, %0.4f, %0.4f, %0.4f ") % czm.elevation_thr_[0] % czm.elevation_thr_[1] %
                 czm.elevation_thr_[2] %
                 czm.elevation_thr_[3]).str());
        ROS_INFO_STREAM((boost::format("flatness_thr_: %0.4f, %0.4f, %0.4f, %0.4f ") % czm.flatness_thr_[0] % czm.flatness_thr_[1] %
                 czm.flatness_thr_[2] %
                 czm.flatness_thr_[3]).str());
    }

    bool validate() {
        return  check(czm.num_zones_ != 4, "Number of zones must be four!") &&\
                check(czm.num_zones_ == czm.num_sectors_each_zone_.size(), "num_zones and length of num_sectors_each_zone must match") &&\
                check(czm.num_zones_ == czm.num_rings_each_zone_.size(), "num_zones and length of num_rings_each_zone must match") &&\
                check(czm.num_zones_ == czm.elevation_thr_.size(), "num_zones and length of elevation_thresholds must match") &&\
                check(czm.num_zones_ == czm.flatness_thr_.size(), "num_zones and length of flatness_thresholds must match");
    }

    std::string mode_;
    bool initialized_;
    bool verbose_;
    double sensor_height_;
    int num_iter_;
    int num_lpr_;
    int num_min_pts_;
    double th_seeds_;
    double th_dist_;
    double th_seeds_v_;
    double th_dist_v_;
    double max_range_;
    double min_range_;
    double uprightness_thr_;
    double adaptive_seed_selection_margin_;
    double RNR_ver_angle_thr_;
    double RNR_intensity_thr_;
    int max_flatness_storage_;
    int max_elevation_storage_;
    bool enable_RNR_;
    bool enable_RVPF_;
    bool enable_TGR_;
    size_t num_rings_of_interest_;
    std::vector<double> min_ranges_;
    std::vector<double> ring_sizes_;
    std::vector<double> sector_sizes_;

    struct CZM
    {
        int num_zones_;
        std::vector<int> num_sectors_each_zone_;
        std::vector<int> num_rings_each_zone_;
        std::vector<double> elevation_thr_;
        std::vector<double> flatness_thr_;
    } czm;

private:
    void reconfigure_callback(const patchworkpp::PatchworkppConfig& config, uint32_t level) {
        verbose_ = config.verbose;
        sensor_height_ = config.sensor_height;
        num_iter_ = config.num_iter;
        num_lpr_ = config.num_lpr;
        num_min_pts_ = config.num_min_pts;
        th_seeds_ = config.th_seeds;
        th_dist_ = config.th_dist;
        th_seeds_v_ = config.th_seeds_v;
        th_dist_v_ = config.th_dist_v;
        uprightness_thr_ = config.uprightness_thr;
        adaptive_seed_selection_margin_ = config.adaptive_seed_selection_margin;
        RNR_ver_angle_thr_ = config.RNR_ver_angle_thr;
        RNR_intensity_thr_ = config.RNR_intensity_thr;
        enable_RNR_ = config.enable_RNR;
        enable_RVPF_ = config.enable_RVPF;
        enable_TGR_ = config.enable_TGR;

        czm.num_zones_ = config.czm_num_zones;
        czm.num_sectors_each_zone_ = convert_string_to_vector<int>(config.czm_num_sectors_each_zone);
        czm.num_rings_each_zone_ = convert_string_to_vector<int>(config.czm_num_rings_each_zone);
        czm.elevation_thr_ = convert_string_to_vector<double>(config.czm_elevation_thresholds);
        czm.flatness_thr_ = convert_string_to_vector<double>(config.czm_flatness_thresholds);

        num_rings_of_interest_ = czm.elevation_thr_.size();

        auto min_range_z2 = (7 * min_range_ + max_range_) / 8.0;
        auto min_range_z3 = (3 * min_range_ + max_range_) / 4.0;
        auto min_range_z4 = (min_range_ + max_range_) / 2.0;

        validate();

        min_ranges_ = {min_range_, min_range_z2, min_range_z3, min_range_z4};
        ring_sizes_ = {(min_range_z2 - min_range_) / czm.num_rings_each_zone_.at(0),
                      (min_range_z3 - min_range_z2) / czm.num_rings_each_zone_.at(1),
                      (min_range_z4 - min_range_z3) / czm.num_rings_each_zone_.at(2),
                      (max_range_ - min_range_z4) / czm.num_rings_each_zone_.at(3)};
        sector_sizes_ = {2 * M_PI / czm.num_sectors_each_zone_.at(0), 2 * M_PI / czm.num_sectors_each_zone_.at(1),
                        2 * M_PI / czm.num_sectors_each_zone_.at(2),
                        2 * M_PI / czm.num_sectors_each_zone_.at(3)};

        ROS_INFO("Updated params");
        initialized_ = true;
    }


    template <typename T>
    std::vector<T> convert_string_to_vector(const std::string& str, char separator = ',') {
        std::vector<T> result;
        std::istringstream iss(str);
        std::string token;
        
        while (std::getline(iss, token, separator)) {
            token = std::regex_replace(token, std::regex("\\s+"), "");
            T num;
            std::stringstream stream(token);
            stream >> num;
            if (stream.fail()) {
                ROS_WARN_STREAM("Can't convert " << token << " to number");
                return result;
            }
        }
        
        return result;
    }


  bool check(bool assertion, std::string description) {
    if (not assertion) {
        ROS_WARN_STREAM(description);
        return false;
    }

    return true;
  }

  dynamic_reconfigure::Server<patchworkpp::PatchworkppConfig> server_;
};

#endif 