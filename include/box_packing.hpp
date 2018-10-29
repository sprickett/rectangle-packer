#pragma once


#include <vector>
#include <limits>
#include <memory>
#include <map>
#include <functional>
struct Size
{
	Size(int width=0,int height=0)
		:width(width),height(height){}
	int width, height;
};
struct Rectangle
{
	Rectangle(void)
		:x(0),y(0),width(0), height(0) {}
	Rectangle(int x, int y,int width = 0, int height = 0)
		:x(x), y(y), width(width), height(height) {}
	int x,y,width,height;
};


class BoxPacker
{
public:	
	typedef std::pair<size_t, Rectangle> Packing;
	typedef std::pair<Size, std::vector<size_t> > Box;
	typedef std::function<bool(const std::vector<Packing>&)> SolutionCallback;
	typedef uint64_t BitMask;


	static constexpr int mask_bits = std::numeric_limits<BitMask>::digits;
	BoxPacker(int width = 0, int height = 0, bool can_rotate = false);
	void insert(Size sz);

	typedef std::vector<Size>::iterator Iter; 
	void insert(const Iter& begin, const Iter& end);
	void pack(SolutionCallback callback= 0, unsigned min_waste = std::numeric_limits<unsigned>::max());
private:


	struct UnpackedBox
	{
		Size size;
		UnpackedBox* other;
		UnpackedBox* next;
		UnpackedBox* prev;
		const Box* box;
		unsigned count;
	};
	struct PackedBox
	{
		unsigned i = 0;
		unsigned j = 0;
		UnpackedBox *box = 0;
		size_t index = -1;
		unsigned waste = 0;
	};

	struct Compare
	{
		bool operator()(const Size& lhs, const Size& rhs) const
		{
			return lhs.height > rhs.height || (!(lhs.height < rhs.height) && lhs.width > rhs.width);
		}
		bool operator()(const Box& lhs, const Box& rhs)
		{
			return this->operator()(lhs.first, rhs.first);
		}
		bool operator()(const UnpackedBox& lhs, const UnpackedBox& rhs)
		{
			return this->operator()(lhs.size, rhs.size);
		}
		bool operator()(const Size& lhs, const Box& rhs)
		{
			return this->operator()(lhs, rhs.first);
		}
		bool operator()(const Box& lhs, const Size& rhs)
		{
			return this->operator()(lhs.first, rhs);
		}
		bool operator()(const UnpackedBox& lhs, const Size& rhs)
		{
			return this->operator()(lhs.size, rhs);
		}
		bool operator()(const Size& lhs, const UnpackedBox& rhs)
		{
			return this->operator()(lhs, rhs.size);
		}
	};

	void show(const UnpackedBox& ub);
	void show(const PackedBox& pb);

	bool makeSolutionCallback(const PackedBox* begin, const PackedBox* end, SolutionCallback& cb);	
	bool scanMask(unsigned& i, unsigned&  j, unsigned& waste, unsigned width, unsigned height);
	bool checkMask(unsigned width, unsigned height, unsigned i, unsigned  j);
	void markMask (unsigned width, unsigned height, unsigned i, unsigned  j);

	bool tryPlacement(PackedBox& p);
	void undoPlacement(const PackedBox& p);
	void dumpMask(void) ;
	void dumpRemaining(void);
	void sortAndAccumulate(void);
	
	void remove(UnpackedBox& d);
	void replace(UnpackedBox& d);

	int width_;
	int height_;
	int stride_;
	int size_;
	unsigned area_;	
	unsigned index_ = 0;
	bool can_rotate_ = false;
	
	std::vector<Box> boxes_;
	std::vector<BitMask> mask_;
	std::vector<Packing> solution_;

	UnpackedBox* box_off; // temp for debugging
};
