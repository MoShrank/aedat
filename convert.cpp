#include "convert.hpp"
#include "dvs_gesture.hpp"

#include <cstddef>
#include <torch/csrc/autograd/python_variable.h>
#include <torch/extension.h>
#include <torch/script.h>
#include <torch/torch.h>
#include <type_traits>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

torch::Tensor
convert_polarity_events(std::vector<AEDAT::PolarityEvent> &polarity_events,
                        const std::vector<int64_t> &tensor_size)
{
  const size_t size = polarity_events.size();
  std::vector<int64_t> indices(3 * size);
  std::vector<int8_t> values;
  const auto max_duration =
      tensor_size.empty()
          ? polarity_events.back().timestamp - polarity_events[0].timestamp
          : tensor_size[0];

  for (size_t idx = 0; idx < size; idx++)
  {
    auto event = polarity_events[idx];
    auto event_time = event.timestamp - polarity_events[0].timestamp;
    //  Break if event is after max_duration
    if (event_time >= max_duration)
    {
      break;
    }

    indices[idx] = event_time;
    indices[size + idx] = event.x;
    indices[2 * size + idx] = event.y;
    values.push_back(event.polarity ? 1 : -1);
  }

  auto index_options = torch::TensorOptions().dtype(torch::kInt64);
  torch::Tensor ind = torch::from_blob(
      indices.data(), {3, static_cast<uint32_t>(size)}, index_options);

  auto value_options = torch::TensorOptions().dtype(torch::kInt8);
  torch::Tensor val = torch::from_blob(
      values.data(), {static_cast<uint32_t>(size)}, value_options);

  auto events =
      tensor_size.empty()
          ? torch::sparse_coo_tensor(ind, val)
          : torch::sparse_coo_tensor(ind, val, torch::IntArrayRef(tensor_size));

  return events.clone();
}

std::vector<torch::Tensor>
convert_polarity(std::vector<AEDAT::PolarityEvent> &polarity_events,
                 const int64_t window_size,
                 const int64_t window_step,
                 const std::vector<double> &scale,
                 const std::vector<int64_t> &image_dimensions)
{
  std::vector<torch::Tensor> event_tensors;
  size_t start = 0;
  size_t idx = 0;
  size_t next_idx = 0;
  bool next_idx_found = false;
  auto last_event = polarity_events.back();
  while (start < last_event.timestamp - window_size)
  {
    auto event = polarity_events[idx];
    size_t start_time = event.timestamp;
    std::vector<int64_t> indices;
    std::vector<int8_t> values;

    while (event.timestamp < start + window_size)
    {
      indices.push_back(static_cast<int64_t>((event.timestamp - start_time) / scale[0]));
      indices.push_back(static_cast<int64_t>(event.x / scale[1]));
      indices.push_back(static_cast<int64_t>(event.y / scale[2]));
      values.push_back(event.polarity ? 1 : -1);
      if (!next_idx_found && (event.timestamp >= start + window_step))
      {
        next_idx = idx;
        next_idx_found = true;
      }
      idx += 1;
      event = polarity_events[idx];
    }

    // create sparse tensor
    auto index_options = torch::TensorOptions().dtype(torch::kInt64);
    torch::Tensor ind = torch::from_blob(
                            indices.data(), {static_cast<uint32_t>(indices.size() / 3), 3}, index_options)
                            .permute({1, 0});

    auto value_options = torch::TensorOptions().dtype(torch::kInt8);
    torch::Tensor val = torch::from_blob(
        values.data(), {static_cast<uint32_t>(indices.size() / 3)}, value_options);
    auto events = torch::sparse_coo_tensor(ind, val, {window_size, image_dimensions[0], image_dimensions[1]});

    event_tensors.push_back(events.clone());

    idx = next_idx;
    start += window_step;
    next_idx_found = false;
  }

  return event_tensors;
}

long int get_total_seconds_of_events(std::vector<AEDAT::PolarityEvent> &events)
{
  uint32_t start = events.front().timestamp;
  uint32_t end = events.back().timestamp;
  auto diff = std::chrono::duration<uint32_t, std::micro>(end - start);
  auto diff_sec = std::chrono::duration_cast<std::chrono::seconds>(diff);

  return diff_sec.count();
}

std::vector<AEDAT::PolarityEvent> get_events_at_second(std::vector<AEDAT::PolarityEvent> &events, int second)
{
  int start_offset = 0;

  auto sec = std::chrono::seconds(second);
  auto micro_sec = std::chrono::duration_cast<std::chrono::microseconds>(sec);

  while (events[start_offset].timestamp < micro_sec.count())
  {
    start_offset += 1;
  }

  std::vector<AEDAT::PolarityEvent> events_in_interval(events.begin(), events.begin() + start_offset);

  return events_in_interval;
}

std::vector<std::vector<AEDAT::PolarityEvent>> split_events(std::vector<AEDAT::PolarityEvent> &events)
{
  std::vector<std::vector<AEDAT::PolarityEvent>> split_events;

  int start_offset = 0;
  int end_offset = 0;
  int cur_sec = 0;

  auto total_seconds = get_total_seconds_of_events(events);

  while (cur_sec <= total_seconds)
  {

    auto sec = std::chrono::seconds(cur_sec);
    auto micro_sec = std::chrono::duration_cast<std::chrono::microseconds>(sec);

    do
    {
      end_offset += 1;
    } while (events[end_offset].timestamp < micro_sec.count());

    auto temp_vec = std::vector<AEDAT::PolarityEvent>(events.begin() + start_offset, events.begin() + end_offset);
    split_events.push_back(temp_vec);

    start_offset = end_offset;
    cur_sec += 1;
  }

  return split_events;
}

std::vector<torch::Tensor> get_frames_from_events(std::vector<AEDAT::PolarityEvent> &events)
{

  std::vector<torch::Tensor> frames;

  int start_offset = 0;
  int end_offset = 0;
  int cur_sec = 0;

  auto total_seconds = get_total_seconds_of_events(events);

  while (cur_sec <= total_seconds)
  {
    auto sec = std::chrono::seconds(cur_sec);
    auto micro_sec = std::chrono::duration_cast<std::chrono::microseconds>(sec);

    do
    {
      end_offset += 1;
    } while (events[end_offset].timestamp < micro_sec.count());

    auto temp_vec = std::vector<AEDAT::PolarityEvent>(events.begin() + start_offset, events.begin() + end_offset);

    torch::Tensor tensors = convert_polarity_events(temp_vec);
    //torch::Tensor aggr_tensor = torch::_sparse_sum(tensors, 0);

    //frames.push_back(aggr_tensor);

    start_offset = end_offset;
    cur_sec += 1;
  }

  std::reverse(frames.begin(), frames.end());

  return frames;
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
  py::class_<AEDAT::PolarityEvent>(m, "PolarityEvent")
      .def("get_valid", &AEDAT::PolarityEvent::get_valid)
      .def("get_x", &AEDAT::PolarityEvent::get_x)
      .def("get_y", &AEDAT::PolarityEvent::get_y)
      .def("get_polarity", &AEDAT::PolarityEvent::get_polarity)
      .def("get_timestamp", &AEDAT::PolarityEvent::get_timestamp);

  py::class_<dvs_gesture::DataSet::DataPoint>(m, "DVSGestureDataPoint")
      .def_readonly("label", &dvs_gesture::DataSet::DataPoint::label)
      .def_readonly("events", &dvs_gesture::DataSet::DataPoint::events);

  py::class_<dvs_gesture::DataSet>(m, "DVSGestureData")
      .def(py::init<>())
      .def(py::init<const std::string &, const std::string &>())
      .def("load", &dvs_gesture::DataSet::load)
      .def_readonly("datapoints", &dvs_gesture::DataSet::datapoints);

  py::class_<AEDAT4::Frame>(m, "AEDAT4Frame")
      .def_readwrite("time", &AEDAT4::Frame::time)
      .def_readwrite("width", &AEDAT4::Frame::width)
      .def_readwrite("height", &AEDAT4::Frame::height)
      .def_readwrite("pixels", &AEDAT4::Frame::pixels);

  py::class_<AEDAT>(m, "AEDAT")
      .def(py::init<>())
      .def(py::init<const std::string &>())
      .def("load", &AEDAT::load)
      .def_readwrite("polarity_events", &AEDAT::polarity_events)
      .def_readwrite("dynapse_events", &AEDAT::dynapse_events)
      .def_readwrite("imu6_events", &AEDAT::imu6_events)
      .def_readwrite("imu9_events", &AEDAT::imu9_events);

  m.def("convert_polarity", &convert_polarity,
        py::arg("polarity_events"),
        py::arg("window_size"),
        py::arg("window_step"),
        py::arg("scale"),
        py::arg("image_dimension"),
        "Converts the AEDAT data into a dense Torch tensor.");

  m.def("get_frames_from_events", &get_frames_from_events,
        py::arg("polarity_events"),
        "Converts events into frame");

  m.def("get_total_seconds_of_events", &get_total_seconds_of_events,
        py::arg("polarity_events"),
        "Get seconds of event");

  m.def("get_events_at_second", &get_events_at_second,
        py::arg("polarity_events"),
        py::arg("second"),
        "Get all events in specific time intervall");

  m.def("split_events", &split_events,
        py::arg("polarity_events"),
        "Splits events");

  m.def("convert_polarity_events", &convert_polarity_events,
        py::arg("polarity_events"),
        py::arg("tensor_size") = std::vector<int64_t>(),
        "Converts the AEDAT data into a sparse Torch tensor. If provided, the "
        "tensor is loaded and shaped after the tensor_size argument");

  py::class_<AEDAT4>(m, "AEDAT4")
      .def(py::init<>())
      .def(py::init<const std::string &>())
      .def("load", &AEDAT4::load)
      .def_readwrite("polarity_events", &AEDAT4::polarity_events)
      .def_readwrite("frames", &AEDAT4::frames);
}