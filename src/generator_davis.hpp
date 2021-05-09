
#include <libcaercpp/devices/davis.hpp>
#include <torch/torch.h>

class GeneratorDavis
{

private:
    libcaer::devices::davis davisHandle;
    bool isClosed = false;

public:
    GeneratorDavis(libcaer::devices::davis);
    void dataStart();
    void dataEnd();
    torch::Tensor next();
}