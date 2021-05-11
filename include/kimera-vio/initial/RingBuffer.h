#pragma once
#include <glog/logging.h>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <vector>

namespace VIO {

// based on
// https://internalpointers.com/post/writing-custom-iterators-modern-cpp
template <typename Value>
struct RingBufferIter {
  using iterator_category = std::bidirectional_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = Value;
  using pointer = value_type*;
  using reference = value_type&;

  RingBufferIter(const std::vector<Value>& buffer,
                 size_t num_measurements,
                 size_t curr_index,
                 value_type const* ptr)
      : m_ptr_(ptr),
        curr_index_(curr_index),
        num_values_(num_measurements),
        begin_ptr_(&(buffer.data()[0])),
        end_ptr_(&(buffer.data()[buffer.size() - 1])) {}

  const value_type& operator*() const {
    CHECK_GE(m_ptr_, begin_ptr_);
    CHECK_LE(m_ptr_, end_ptr_);
    return *m_ptr_;
  }

  pointer operator->() { return m_ptr_; }

  RingBufferIter& operator++() {
    m_ptr_++;
    if (m_ptr_ > end_ptr_) {
      m_ptr_ = begin_ptr_;
    }
    return *this;
  }

  RingBufferIter operator++(int) {
    RingBufferIter tmp = *this;
    ++(*this);
    return tmp;
  }

  inline friend bool operator==(const RingBufferIter& a,
                                const RingBufferIter& b) {
    return a.m_ptr_ == b.m_ptr_;
  }

  inline friend bool operator!=(const RingBufferIter& a,
                                const RingBufferIter& b) {
    return !(a == b);
  }

 private:
  value_type const* m_ptr_;
  const size_t curr_index_;
  const size_t num_values_;
  value_type const* begin_ptr_;
  value_type const* end_ptr_;
};

template <typename T>
class RingBuffer {
 public:
  using Iterator = RingBufferIter<T>;
  using value_type = T;

  explicit RingBuffer(size_t buffer_size)
      : size_(buffer_size),
        num_values_(0),
        curr_index_(0),
        values_(buffer_size + 1) {}

  /**
   * @brief reset the buffer to be empty
   */
  void clear() {
    values_ = std::vector<T>(size_ + 1);
    num_values_ = 0;
    curr_index_ = 0;
  }

  /**
   * @brief get the element at the index specified.
   *
   * @note no specific bounds checking is performed,
   *       but all memory accesses are safe
   */
  T operator[](size_t index) const {
    if (num_values_ < size_) {
      return values_.at(index);
    }

    if (index < size_ - curr_index_) {
      return values_.at(curr_index_ + index + 1);
    }

    return values_.at(index - (size_ - curr_index_));
  }

  /**
   * @brief get the current number of elements in the buffer
   */
  inline size_t size() const { return num_values_; }

  /**
   * @brief get whether the buffer is fulll
   */
  inline bool full() const { return size_ == num_values_; }

  /**
   * @brief get whether the buffer is empty
   */
  inline bool empty() const { return num_values_ == 0; }

  /**
   * @brief add a new value at the end of the buffer.
   * @note will drop the earliest value if the buffer
   *       is full
   */
  void push(const T& value) {
    values_[curr_index_] = value;
    if (curr_index_ == size_) {
      curr_index_ = 0;
    } else {
      curr_index_++;
    }
    if (num_values_ < size_) {
      num_values_++;
    }
  }

  /**
   * @brief get an iterator at the first element
   */
  inline Iterator begin() const {
    return Iterator(
        values_, num_values_, curr_index_, &(values_.data()[getBeginIndex()]));
  }

  /**
   * @brief get an iterator at the last element
   */
  inline Iterator end() const {
    return Iterator(
        values_, num_values_, curr_index_, &(values_.data()[getEndIndex()]));
  }

  /**
   * @brief get the first element (undefined behavior when empty)
   */
  inline T front() const { return values_[getBeginIndex()]; }

  /**
   * @brief get the last element (undefined behavior when empty)
   */
  inline T back() const { return values_[getNewestIndex()]; }

  /**
   * @brief display buffer statistics
   */
  friend std::ostream& operator<<(std::ostream& out, const RingBuffer& buffer) {
    out << "buffer<(curr=" << buffer.curr_index_
        << ", size=" << buffer.num_values_ << ", max_size=" << buffer.size_
        << ")>";
    return out;
  }

 private:
  // oldest message in buffer
  size_t getBeginIndex() const {
    if (num_values_ < size_) {
      return 0;
    }

    return (curr_index_ == size_) ? 0 : curr_index_ + 1;
  }

  // newest message in buffer
  size_t getNewestIndex() const {
    if (num_values_ == 0) {
      return 0;
    }
    return (curr_index_ == 0) ? size_ : curr_index_ - 1;
  }

  // index to stop iteration on
  size_t getEndIndex() const {
    if (num_values_ == 0) {
      return 0;
    }
    return curr_index_;
  }

  size_t size_;
  size_t num_values_;
  size_t curr_index_;
  std::vector<T> values_;
};

namespace utils {

/**
 * @brief get the mean of a iterable sequence
 * @param seq input sequence
 * @param accessor function to map T to a double
 */
template <typename T>
double mean(const T& seq,
            std::function<double(const typename T::value_type&)> accessor) {
  if (seq.size() == 0) {
    return 0;
  }
  const double total =
      std::accumulate(std::begin(seq),
                      std::end(seq),
                      0.0,
                      [&](double sum, const typename T::value_type& v) {
                        return sum + accessor(v);
                      });
  return total / seq.size();
}

/**
 * @brief get the variance of a iterable sequence
 * @param seq input sequence
 * @param accessor function to map T to a double
 */
template <typename T>
double variance(const T& seq,
                std::function<double(const typename T::value_type&)> accessor) {
  if (seq.size() == 0) {
    return 0.0;
  }

  double x_bar = mean(seq, accessor);
  double x_squared = 0.0;
  for (const auto& x : seq) {
    x_squared += accessor(x) * accessor(x);
  }
  return (x_squared / seq.size()) - (x_bar * x_bar);
}

/**
 * @brief get the full cross-correlation between two sequences
 * @param seq_a first input sequence
 * @param seq_b second input sequence
 * @param accessor function to map T to a double
 *
 * We follow the same convention as
 * https://numpy.org/doc/stable/reference/generated/numpy.correlate.html,
 * except that we use the 'full' mode
 *
 * @return correlation between seq_a and seq_b
 */
template <typename T>
std::vector<double> crossCorrelation(
    const T& seq_a,
    const T& seq_b,
    std::function<double(const typename T::value_type&)> accessor,
    bool mean_removal = false) {
  double mean_a = 0.0;
  double mean_b = 0.0;
  if (mean_removal) {
    mean_a = mean(seq_a, accessor);
    mean_b = mean(seq_b, accessor);
  }
  const size_t N = seq_a.size() + seq_b.size() - 1;
  std::vector<double> correlation(N);
  for (size_t k = 0; k < N; ++k) {
    int64_t offset =
        static_cast<int64_t>(seq_a.size()) - static_cast<int64_t>(N) + k;
    size_t start = (offset < 0) ? std::abs(offset) : 0;
    size_t end = (k < seq_a.size()) ? seq_b.size()
                                    : seq_b.size() - (k - seq_a.size()) - 1;

    double corr = 0.0;
    for (size_t n = start; n < end; ++n) {
      // Note that mean_a and mean_b are 0 when mean_removal is false,
      // which doesn't affect the computation here
      corr += (accessor(seq_a[n + offset]) - mean_a) *
              (accessor(seq_b[n]) - mean_b);
    }
    correlation[k] = corr;
  }

  return correlation;
}

}  // namespace utils

}  // namespace VIO
