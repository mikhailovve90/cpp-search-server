#pragma once
#include <vector>
template <typename Iterator>
class IteratorRange{
public:
  IteratorRange(){};

  IteratorRange(Iterator start_r, Iterator end_r, size_t size_r) : start_iter_(start_r), end_iter_(end_r),
                                                size_range_(size_r){};



  size_t size() const {return size_range_;};
  auto begin() const {return start_iter_;};
  auto end() const {return end_iter_;};

private:

  Iterator start_iter_ ;
  Iterator end_iter_;
  size_t size_range_ = 0;
};

template <typename Iterator>
class Paginator {
  public:
    Paginator(Iterator start_r, Iterator end_r, size_t size ){
        //auto iter_for_end = distance(start_r, end_r)
        while(distance(start_r, end_r) > 0){
          auto old_begin_iter = start_r;
          if(distance(start_r, end_r) < size) advance(start_r, distance(start_r, end_r));
          else advance(start_r, size);
          auto i_p = IteratorRange<Iterator>(old_begin_iter, start_r, size);
          pages_.push_back(i_p);
        }
    };

    auto begin() const {return pages_.begin();};
    auto end() const {return pages_.end();};
    size_t size() const {return pages_.size();};

  private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
  return Paginator(c.begin(), end(c), page_size);
}

