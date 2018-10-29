#include "box_packing.hpp"
#include <algorithm>
#include <tuple>


#include <iostream>
#include <iomanip>

//namespace { //anon
//
//	struct UnpackedBox
//	{
//		Size size;
//		UnpackedBox* other;
//		UnpackedBox* next;
//		UnpackedBox* prev;
//		const BoxPacker::Box* box;
//		unsigned count;
//	};
//	struct PackedBox
//	{
//		unsigned i = 0;
//		unsigned j = 0;
//		UnpackedBox *box = 0;
//		size_t index = -1;
//		unsigned waste = 0;
//	};
//
//	struct Compare
//	{
//		bool operator()(const Size& lhs, const Size& rhs) const
//		{
//			return lhs.height > rhs.height || (!(lhs.height < rhs.height) && lhs.width > rhs.width);
//		}
//		bool operator()(const BoxPacker::Box& lhs, const BoxPacker::Box& rhs)
//		{
//			return this->operator()(lhs.first, rhs.first);
//		}
//		bool operator()(const UnpackedBox& lhs, const UnpackedBox& rhs)
//		{
//			return this->operator()(lhs.size, rhs.size);
//		}
//		bool operator()(const Size& lhs, const BoxPacker::Box& rhs)
//		{
//			return this->operator()(lhs, rhs.first);
//		}
//		bool operator()(const BoxPacker::Box& lhs, const Size& rhs)
//		{
//			return this->operator()(lhs.first, rhs);
//		}
//		bool operator()(const UnpackedBox& lhs, const Size& rhs)
//		{
//			return this->operator()(lhs.size, rhs);
//		}
//		bool operator()(const Size& lhs, const UnpackedBox& rhs)
//		{
//			return this->operator()(lhs, rhs.size);
//		}
//	};
//
//	void show(const UnpackedBox& ub);
//	void show(const PackedBox& pb);
//
//	bool makeSolutionCallback(const PackedBox* begin, const PackedBox* end, BoxPacker::SolutionCallback& cb);
//	bool scan_mask(unsigned& i, unsigned&  j, unsigned& waste, unsigned width, unsigned height);
//	bool check_mask(unsigned width, unsigned height, unsigned i, unsigned  j);
//	void mark_mask(unsigned width, unsigned height, unsigned i, unsigned  j);
//
//	bool tryPlacement(PackedBox& p);
//	void undoPlacement(const PackedBox& p);
//	void dumpMask(void);
//	void dumpRemaining(void);
//	void sortAndAccumulate(void);
//
//	void remove(UnpackedBox& d);
//	void replace(UnpackedBox& d);
//} // anon

static inline void increment(unsigned& i, unsigned& j, int x)
{
	j += x;
	i += j / BoxPacker::mask_bits;
	j &= BoxPacker::mask_bits - 1;
}


BoxPacker::BoxPacker(int width, int height, bool can_rotate)
	:width_(width)
	,height_(height)
	,stride_((width + mask_bits) / mask_bits )
	,size_((height+1) * stride_)
	,mask_(size_)
	,can_rotate_(can_rotate)
{
	int i = width_ / mask_bits;
	int j = width_ & (mask_bits - 1);
	int w = stride_ * mask_bits;
	markMask(w-width_, height_,i ,j);
	markMask(w, 1, stride_*height_, 0);
}

void BoxPacker::insert(Size sz)
{
	if (can_rotate_ && sz.width < sz.height) // handing not important 
		std::swap(sz.width, sz.height);
	boxes_.push_back( Box(sz, std::vector<size_t>(1, index_++)) );
}


void BoxPacker::insert(const Iter & begin, const Iter & end)
{
	boxes_.reserve(boxes_.size() + std::distance(begin, end));
	for (auto i = begin; i != end; ++i)
		insert(*i);




}

void BoxPacker::sortAndAccumulate(void)
{
	auto begin = boxes_.begin();
	auto end = boxes_.end();
	if (end == begin) 
		return;

	// combine duplicate sizes into one   
	Compare comp;
	std::sort(begin, end, comp);
	auto w = begin;
	for (auto r = w + 1; r != end; ++r)
	{
		if (comp(*w, *r)) // not equal, increment and swap 
			std::swap(*++w, *r); 
		else // equal, copy in indices
			w->second.insert(w->second.end(), r->second.begin(), r->second.end());
	}
	boxes_.erase(++w, end); 
}

struct DumbCB
{
	bool operator()(const std::vector<BoxPacker::Packing>&) { return true; }
};

void BoxPacker::pack(SolutionCallback callback, unsigned min_waste)
{	
	if (boxes_.empty())
		return;
	if (!callback)
		callback = DumbCB();

	std::vector<UnpackedBox> unpacked_;
	std::vector<PackedBox> packed_;

	sortAndAccumulate();	
	unsigned estimate = boxes_.size();
	if (can_rotate_)
		estimate *= 2;

	unpacked_.reserve(estimate + 1);
	unpacked_.resize(1);

	for (const Box& bx : boxes_)
	{
		const Size& sz = bx.first;
		if (sz.width <= 0 || sz.height <= 0)
			continue;

		if (sz.width <= width_ && sz.height <= height_)
		{
			unpacked_.push_back(UnpackedBox());
			auto& u = unpacked_.back();
			u.size = sz;
			u.other = nullptr;
			u.count = bx.second.size();
			u.box = &bx;
		}

		if (can_rotate_ && sz.height != sz.width && sz.height <= width_ && sz.width <= height_)
		{
			unpacked_.push_back(UnpackedBox());
			auto& u = unpacked_.back();
			u.size.width = sz.height;
			u.size.height = sz.width;
			u.other = nullptr;
			u.count = bx.second.size();
			u.box = &bx;
		}
	}

	if (can_rotate_)
	{
		Compare comp;
		std::sort(unpacked_.begin()+1, unpacked_.end(), comp);
		for (auto& u : unpacked_)
		{
			Size sz(u.size.height, u.size.width); // swapped
			if (sz.height >= sz.width)
				continue;
			auto iu = std::lower_bound(unpacked_.begin()+1, unpacked_.end(), sz, comp);
			if (iu == unpacked_.end() || comp(iu->size, sz))
				continue;
			u.other = &*iu;
			iu->other = &u;
		}
	}
	
	int min_width = std::numeric_limits<int>::max();
	int min_height = std::numeric_limits<int>::max();
	unsigned num_sizes = 0;
	unsigned total_area = 0;
	for (auto iu = unpacked_.begin()+1; iu< unpacked_.end();++iu)
	{
		UnpackedBox& u = *iu;
		u.next = &u + 1;
		u.prev = &u - 1;
		min_width = std::min(min_width, u.size.width);
		min_height = std::min(min_height, u.size.height);
		if (u.other && u.size.width <= u.size.height)
			continue;
		num_sizes += u.count;
		total_area += u.size.width*u.size.height*u.count;
	}
	{ // start end dimension
		UnpackedBox& u = unpacked_.front();
		u.next = &u + 1;
		u.prev = &unpacked_.back();
		u.count = 0;
		u.other = nullptr;
		u.box = nullptr;
		u.prev->next = &u;
	}
	
	packed_.resize(num_sizes);
	box_off = &unpacked_[0];

	area_ = 0;
	
	unsigned max_area = 0; 
	unsigned bed_area = width_ * height_;
	unsigned max_waste = 0;

	//for (auto& up : unpacked_)
	//	show(up);
	//return;

	std::cout << "total bed area: " << bed_area << "\n";
	std::cout << "total area: " << total_area<< "\n";
	std::cout << "total boxes: " << num_sizes << "\n";
	std::cout << "min width: " <<min_width << "\n";
	std::cout << "min height: " << min_height << "\n";

	packed_.front().box = &unpacked_[0];
	for (int depth = 0; depth >= 0; )
	{
		PackedBox& p = packed_[depth];
	

		if (p.box->box) // undo any previous placement (common case)
		{
			undoPlacement(p);
		}
		else if (scanMask(p.i, p.j, p.waste, min_width, unpacked_[0].prev->size.height) == false || p.waste >= min_waste) // no position found
		{
			if (area_ > max_area && makeSolutionCallback(&packed_[0], &packed_[depth], callback))
				min_waste = 0; // exit early
			
			max_area = std::max(max_area, area_);
			min_waste = std::min(min_waste, bed_area - area_);

			--depth;
			continue;
		}

		// loop through remaining sizes until one 
		// that can be placed are found or we run out
		do { p.box = p.box->next; } while (p.box->box && !tryPlacement(p));

		if (!p.box->box) // no placement found 
		{
			increment(p.i, p.j, 1); // move forward one 
			++p.waste;
		}
		else if (depth < num_sizes - 1) // placement found and placements left 
		{
			++depth;
			PackedBox& q = packed_[depth];
			q.box = &unpacked_[0]; // reset loop
			q.j = p.j;
			q.i = p.i;
			q.waste = p.waste;
			increment(q.i, q.j, p.box->size.width); // move forward past placement
		}
		else // last placement found
		{
			std::cout << "full solution: { area: " << area_ << "  waste: " << p.waste << "  total: " << area_ + p.waste << " } " << depth << "\n";
			if (makeSolutionCallback(&packed_[0], &packed_[depth], callback))
				min_waste = 0;
			min_waste = std::min(min_waste, p.waste);
			max_area = area_;
			undoPlacement(p);
			--depth;
		}

	}
	std::cout << "max waste: " << max_waste << std::endl;
	//dumpRemaining();


}


bool BoxPacker::makeSolutionCallback(const PackedBox* begin, const PackedBox* end, SolutionCallback& cb)
{
	solution_.resize(end-begin);
	auto pk = solution_.begin();
	for (auto pb = begin; pb<end; ++pb, ++pk)
	{
		pk->first = pb->index;
		pk->second.y = pb->i / stride_;
		pk->second.x = (pb->i % stride_) * mask_bits + pb->j;
		pk->second.width = pb->box->size.width;
		pk->second.height = pb->box->size.height;
	}
	return cb(solution_);
}

bool BoxPacker::scanMask(unsigned & i, unsigned & j, unsigned & waste, unsigned width, unsigned height)
{	
	unsigned max_i = (height_+1-height)*stride_;
	//std::cout << "min: " << min << "\n";
	while (i < max_i)
	{
		if (checkMask(width, height, i, j)) 
			return true;

		//std::cout << "check mask fail\n";
		//std::cout << "A i: " << i <<"  j: "<<j<<"\n";
		// move forward until we find an obstacle
		while (i < max_i)
		{
			for (BitMask m = mask_[i]; j < mask_bits; ++j, ++waste)
				if ((m & (BitMask(1) << j)) != 0)
					break;
			if (j < mask_bits)
				break;
			//std::cout << "bits all clear\n";
			//std::cout << "B i: " << i << "  j: " << j << "\n";
			j = 0;			
			for (++i; i < max_i && mask_[i] == 0; ++i)
				waste += mask_bits;	
		}
		//std::cout << "C i: " << i << "  j: " << j << "\n";
		// move to one past obstacle
		while (i < max_i)
		{
			for (BitMask m = mask_[i]; j < mask_bits; ++j)
					if ((m & (BitMask(1) << j)) == 0)
						break;
			if (j < mask_bits)
				break;	
			//std::cout << "bits all set\n";
			//std::cout << "D i: " << i << "  j: " << j << "\n";
			j = 0;
			for (++i; i < max_i && mask_[i] == BitMask(-1); ++i) {};
		}
		//std::cout << "E i: " << i << "  j: " << j << "\n";		
	}
	return false;
}

bool BoxPacker::checkMask(unsigned width, unsigned height, unsigned i, unsigned  j)
{
	if (i + stride_ * height >= size_)
		return false;
	unsigned je = width + j;
	unsigned ct = (je-1) / mask_bits;
	je &= (mask_bits - 1);
	BitMask lo = BitMask(-1) << j;
	BitMask hi = BitMask(-1) >> (mask_bits - je);//?????
	if (!ct)
		lo &= hi, hi = 0;

	const BitMask* M = &mask_[i];
	const BitMask* Me = M + height * stride_;
	for (unsigned x; M < Me; M += stride_)
	{
		if (M[0] & lo)
			return false;
		if (M[ct] & hi)
			return false;
		for (unsigned x = 1; x < ct; ++x)
			if (M[x])
				return false;
	}
	return true;
}

void BoxPacker::markMask(unsigned  width, unsigned  height, unsigned  i, unsigned  j)
{
	unsigned je = width + j;
	unsigned ct = (je-1) / mask_bits;
	je &= (mask_bits - 1);
	BitMask lo = BitMask(-1) << j;
	BitMask hi = BitMask(-1) >> (mask_bits - je);//?????
	if (!ct)
		lo &= hi, hi = 0;

	BitMask* M = &mask_[i];
	BitMask* Me = M + height*stride_;
	for (; M < Me; M += stride_)
	{
		M[0] ^= lo;
		for (unsigned x = 1; x < ct; ++x)
			M[x] ^= BitMask(-1);
		M[ct] ^= hi;			
	}
}

void BoxPacker::remove(UnpackedBox& d)
{
	--d.count;
	if (!d.count)
	{
		d.next->prev = d.prev;
		d.prev->next = d.next;
	}	
}
void BoxPacker::replace(UnpackedBox& d)
{
	if (!d.count)
	{
		d.next->prev = &d;
		d.prev->next = &d;
	}
	++d.count;
}

//void BoxPacker::isolate(Dimension & d)
//{
//	d.next->prev = d.prev;
//	d.prev->next = d.next;
//	d.next = &d;
//	d.prev = &d;
//}

bool BoxPacker::tryPlacement(PackedBox& p)
{
	auto& bx = *p.box;
	if (!checkMask(bx.size.width, bx.size.height, p.i, p.j))
		return false;
	
	markMask(bx.size.width, bx.size.height, p.i, p.j);
	area_ += bx.size.width * bx.size.height;

	remove(bx);
	if(bx.other)
		remove(*bx.other);

	p.index = bx.box->second[bx.count];
	return true;
}

void BoxPacker::undoPlacement(const PackedBox& p)
{
	auto& bx = *p.box;

	markMask(bx.size.width, bx.size.height, p.i, p.j);
	area_ -= bx.size.width * bx.size.height;
	//std::cout << "PLACE " << w.dimension << " x " << h.dimension << " @ " << p.i << ":" << p.j << "\n";
	//dumpMask();

	// remove placed from free dimensions
	replace(bx);
	if (bx.other)
		replace(*bx.other);
}

void BoxPacker::dumpMask(void)
{
	//return;
	std::cout << std::hex;
	for (unsigned i = 0; i < size_;)
	{
		for (unsigned ie = i + stride_; i < ie; ++i)
		{
			BitMask m = mask_[i];
			//for (unsigned j = 0; j < mask_bits / 4; ++j, m>>=4)
			//	std::cout << (m & 0xF);
			for (unsigned j = 0; j < mask_bits; ++j, m >>= 1)
				std::cout << (m & 0x1);
		}
		std::cout << "\n";
	}
	std::cout << std::dec;
}

void BoxPacker::dumpRemaining(void)
{
	//Dimension * const D = &dims_[0];
	//Dimension *d = D;
	//while((d=d->next)!=D)
	//{
	//	std::cout << d->dimension
	//		<< "*" << d->other->dimension
	//		<< " " << d->count
	//		<< " " << d->other - D
	//		<< " " << d->prev - D
	//		<< " " << d->next - D
	//		<< "\n";		
	//}
}
void BoxPacker::show(const UnpackedBox& ub)
{
	using std::setw;
	std::cout << "U"
		<< " {" << setw(4) << ub.size.width 
		<< "," << setw(4)  << ub.size.height << "} "
		<< ub.count << " :"		
		<< setw(3) << (ub.box? ub.box - &boxes_[0]:-1)<< " :"
		<< ub.prev - box_off<< ":"
		<< ub.next - box_off << ":"
		<< (ub.other? ub.other - box_off:-1) << "\n";
}
void BoxPacker::show(const PackedBox& pb)
{
	using std::setw;
	std::cout << setw(4) << pb.i 
		<< "," << setw(4) << pb.j 
		<< ":" << setw(6) << pb.waste 
		<< " : " ;
	if (pb.box)
		show(*pb.box);
	else
		std::cout << "\n";
}