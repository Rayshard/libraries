#include <iostream>
#include "json.h"

using namespace cpplib::json;

int main(int _argc, char* _argv[])
{
    JSONObject object = {
        {"myObject",
        JSONObject({
            {"name", JSONString("Rayshard")}
        })
        },
        {"myArray",
        JSONArray({
            JSONNumber(123),
            true,
            false,
            JSONNull(),
            JSONArray({
                JSONNumber(123.5),
                true,
                false,
                JSONNull()
            })
        })
        }
    };

    std::cout << ToString(object) << std::endl;
    std::stringstream ss(ToString(object));
    std::cout << ToString(FromStream(ss)) << std::endl;
    return 0;
}