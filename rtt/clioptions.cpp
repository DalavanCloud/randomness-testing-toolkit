#include "rtt/clioptions.h"

namespace rtt {

CliOptions CliOptions::getInstance(int argc , char * argv[]) {
    CliOptions options;
    bool batterySet = false;
    int test = -1;

    for(int i = 1; i < argc; i += 2) {
        if(i == (argc - 1))
            throw RTTException(options.objectInfo ,
                               "invalid usage of command-line arguments");

        /* Battery option */
        if(strcmp(argv[i] , "-b") == 0) {
            if(batterySet || argv[i + 1][0] == '-')
                throw RTTException(options.objectInfo ,
                                   "can't set \"-b\" option multiple times or without any value");

            if(strcmp(argv[i + 1] , "dieharder") == 0)
                options.battery = Constants::Battery::DIEHARDER;
            else if(strcmp(argv[i + 1] , "nist_sts") == 0)
                options.battery = Constants::Battery::NIST_STS;
            else if(strcmp(argv[i + 1] , "tu01_smallcrush") == 0)
                options.battery = Constants::Battery::TU01_SMALLCRUSH;
            else if(strcmp(argv[i + 1] , "tu01_crush") == 0)
                options.battery = Constants::Battery::TU01_CRUSH;
            else if(strcmp(argv[i + 1] , "tu01_bigcrush") == 0)
                options.battery = Constants::Battery::TU01_BIGCRUSH;
            else if(strcmp(argv[i + 1] , "tu01_rabbit") == 0)
                options.battery = Constants::Battery::TU01_RABBIT;
            else if(strcmp(argv[i + 1] , "tu01_alphabit") == 0)
                options.battery = Constants::Battery::TU01_ALPHABIT;
            else if(strcmp(argv[i + 1] , "tu01_blockalphabit") == 0)
                options.battery = Constants::Battery::TU01_BLOCK_ALPHABIT;
            else
                throw RTTException(options.objectInfo ,
                                   "unknown battery set: " + (std::string)argv[i + 1]);
            batterySet = true;

        }
        /* Input binary data option */
        else if(strcmp(argv[i] , "-f") == 0) {
            if(!options.inputDataPath.empty() || argv[i + 1][0] == '-')
                throw RTTException(options.objectInfo ,
                                   "can't set \"-b\" option multiple times or without any value");
            options.inputDataPath = argv[i + 1];
        }
        /* Custom input config option */
        else if(strcmp(argv[i] , "-c") == 0) {
            if(!options.inputCfgPath.empty() || argv[i + 1][0] == '-')
                throw RTTException(options.objectInfo ,
                                   "can't set \"-c\" option multiple times or without any value");
            options.inputCfgPath = argv[i + 1];
        }
        /* Test option */
        else if(strcmp(argv[i] , "-t") == 0) {
            if(test != -1 || argv[i + 1][0] == '-')
                throw RTTException(options.objectInfo ,
                                   "can't set \"-t\" option multiple times or without any value");
            test = Utils::strtoi(argv[i + 1]);
        }
        /* MySql option (temporary) - sets that db output storage is used and sets experiment id. */
        else if(strcmp(argv[i] , "--mysql") == 0) {
            if(options.mysqlEid != 0 || argv[i + 1][0] == '-')
                throw RTTException(options.objectInfo ,
                                   "can't set \"--mysql\" option multiple times or without any value");
            options.mysqlEid = Utils::strtoi(argv[i + 1]);
        }
        /* None of the above, error */
        else {
            throw RTTException(options.objectInfo ,
                               "unknown option used: " + (std::string)argv[i]);
        }
    }
    /* Sanity checks */
    if(!batterySet)
        throw RTTException(options.objectInfo ,
                           "option \"-b\" must be correctly set in arguments");
    if(options.inputDataPath.empty())
        throw RTTException(options.objectInfo ,
                           "option \"-f\" must be set in arguments");
    if(options.inputCfgPath.empty())
        throw RTTException(options.objectInfo ,
                           "options \"-c\" must be correctly set in arguments");

    if(!Utils::fileExist(options.inputDataPath))
        throw RTTException(options.objectInfo ,
                           Strings::ERR_FILE_OPEN_FAIL + options.inputDataPath);

    if(test >= 0)
        options.testConsts.push_back(test);
    std::sort(options.testConsts.begin(), options.testConsts.end());

    return options;
}

Constants::Battery CliOptions::getBatteryId() const {
    return battery;
}

std::string CliOptions::getInputCfgPath() const {
    return inputCfgPath;
}

std::vector<int> CliOptions::getTestConsts() const {
    return testConsts;
}

std::string CliOptions::getInputDataPath() const {
    return inputDataPath;
}

std::uint64_t CliOptions::getMysqlEid() const
{
    return mysqlEid;
}

std::string CliOptions::getUsage() {
    std::stringstream ss;
    ss << "\n[USAGE] Randomness Testing Toolkit accepts following options.\n";
    ss << "    -b  Followed with battery that will be used. Following batteries\n";
    ss << "        are accepted: \"dieharder\", \"nist_sts\", \"tu01_smallcrush\",\n";
    ss << "        \"tu01_crush\", \"tu01_bigcrush\", \"tu01_rabbit\",\n";
    ss << "        \"tu01_alphabit\" and \"tu01_blockalphabit\".\n";
    ss << "    -f  Followed with path to input binary data that will be analysed by battery.\n";
    ss << "    -c  Followed with path to config file.\n";
    ss << "    -t  Followed with constant of test in battery that will be used in testing.\n";
    ss << "    -h  Or no arguments will bring up this message.\n\n";
    return ss.str();
}

} // namespace rtt
