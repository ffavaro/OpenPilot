/**
 * \file hardwareSensorAbstract.hpp
 *
 * Header file for hardware sensors
 *
 * \date 18/06/2010
 * \author croussil
 *
 * \ingroup rtslam
 */

#ifndef HARDWARE_SENSOR_HPP_
#define HARDWARE_SENSOR_HPP_

#include "kernel/threads.hpp"

#include "jmath/indirectArray.hpp"

#include "rtslam/rawAbstract.hpp"

namespace jafar {
namespace rtslam {


struct RawInfo
{ unsigned id; double timestamp; double arrival;
	RawInfo(unsigned id, double timestamp, double arrival): id(id), timestamp(timestamp), arrival(arrival) {}
	RawInfo() {}
};
struct RawInfos
	{ std::vector<RawInfo> available; RawInfo next; double process_time; bool integrate_all; };

struct RawVec
{
	jblas::vec data;
	double arrival;
	RawVec(unsigned n): data(n), arrival(0.) {}
	RawVec() {}
};

namespace hardware {

//namespace ublas = boost::numeric::ublas;

#if 0
class HardwareSensorAbstract;
typedef boost::shared_ptr<HardwareSensorAbstract> hardware_sensor_ptr_t;
#endif

#if 1

inline double extractRawTimestamp(raw_ptr_t &raw) { return raw->timestamp; }
inline double extractRawTimestamp(RawVec &raw) { return raw.data(0); }
inline double extractRawArrival(raw_ptr_t &raw) { return raw->arrival; }
inline double extractRawArrival(RawVec &raw) { return raw.arrival; }

/**
	Generic implementation of hardware sensor based on ring buffer.
	You need to inherit this class and start in the constructor a thread that will
	read the sensor, fill the ring buffer, and notify the condition variable when
	a new reading arrives.
	
	TODO should be improved to be able to not fail if it overflows and that it should not
	be fatal but just throw away oldest data. 3 policies on overflow:
	fail, ignore/loop, block (for offline)
	
	@ingroup rtslam
*/
template<typename T>
class HardwareSensorAbstract
{
	public:
		typedef ublas::vector<T> VecT;
		typedef ublas::vector_indirect<VecT> VecIndT;
	
	private:
		int write_pos; /// next position where to write, oldest available reading
		int read_pos;  /// oldest position not released (being read or not read at all)
		bool buffer_full; /// when read_pos = write_pos, tells whether the buffer is full or empty
		bool read_pos_used;  /// current read_pos is being used
		
	protected:
		kernel::VariableCondition<int> &condition; /// to notify when new data is available
		kernel::VariableCondition<int> index; /// index of used data
		boost::mutex mutex_data; /// mutex for using this object
		boost::condition_variable cond_offline_full;
		boost::condition_variable cond_offline_freed;
		int data_count; /// image count since last image read
		int last_sent_pos; /// position of the last raw sent
		bool no_more_data;
		double timestamps_correction;
		double data_period;
		double arrival_delay;
		
		int bufferSize; /// size of the ring buffer
		VecT buffer; /// the ring buffer
		
		int getWritePos(bool locked = false) {
			if (isFull(locked)) JFR_ERROR(RtslamException, RtslamException::GENERIC_ERROR, "buffer of hardware is full"); // FIXME chose policty when full
			// don't need to lock, because will only be used and modified by writer
			return write_pos;
		}
		void incWritePos(bool locked = false) {
			boost::unique_lock<boost::mutex> l(mutex_data, boost::defer_lock_t()); if (!locked) l.lock();
			++write_pos;
			if (write_pos >= bufferSize) write_pos = 0;
			if (write_pos == read_pos) buffer_full = true; // full
			++data_count;
		}
		int getFirstUnreadPos() {
			/// \warning check that buffer is not empty before
			// don't need to lock, because will only be used and modified by reader
			if (!read_pos_used) return read_pos;
			if (read_pos != bufferSize-1) return read_pos+1; else return 0;
		}
		int getLastUnreadPos(bool locked = false) {
			/// \warning check that buffer is not empty before
			boost::unique_lock<boost::mutex> l(mutex_data, boost::defer_lock_t()); if (!locked) l.lock();
			return (write_pos == 0 ? bufferSize-1 : write_pos-1);
		}
		/// release until id, excluding id
		void releaseUntil(unsigned id, bool locked = false) {
			boost::unique_lock<boost::mutex> l(mutex_data, boost::defer_lock_t()); if (!locked) l.lock();
			read_pos = id;
			read_pos_used = true;
			if (getFirstUnreadPos() == write_pos) buffer_full = false;
			l.unlock();
			cond_offline_freed.notify_all();
			// cannot be full as id is not released
		}
		/// release until id, including id
		void release(unsigned id, bool locked = false) {
			boost::unique_lock<boost::mutex> l(mutex_data, boost::defer_lock_t()); if (!locked) l.lock();
			if (id != (unsigned)(bufferSize-1)) read_pos = id+1; else read_pos = 0;
			if (write_pos == read_pos) buffer_full = false; // empty
			read_pos_used = false;
			l.unlock();
			cond_offline_freed.notify_all();
		}
		bool isFull(bool locked = false)
		{
			boost::unique_lock<boost::mutex> l(mutex_data, boost::defer_lock_t()); if (!locked) l.lock();
			return (read_pos == write_pos && buffer_full);
		}
		bool isEmpty(bool locked = false)
		{
			boost::unique_lock<boost::mutex> l(mutex_data, boost::defer_lock_t()); if (!locked) l.lock();
			return (getFirstUnreadPos() == write_pos && !buffer_full);
		}
		
	public:
		/** Constructor
			@param condition to notify when new data is available
		*/
		HardwareSensorAbstract(kernel::VariableCondition<int> &condition, unsigned bufferSize):
			write_pos(0), read_pos(0), buffer_full(false), read_pos_used(false),
		  condition(condition), index(-1),
		  data_count(0), no_more_data(false), timestamps_correction(0.0),
		  bufferSize(bufferSize), buffer(bufferSize)
		{}
		virtual void start() = 0; ///< start the acquisition thread, once the object is configured
		void setSyncConfig(double timestamps_correction = 0.0)
			{ this->timestamps_correction = timestamps_correction; }
		/**
			Provides approximate informations about the timings of data
			@param data_period the period at which the data are arriving
			@param arrival_delay the delay between the moment we get a data and its real date.
			This is a starting point that must be overestimated,
			it may be estimated more precisely afterwards.
		*/
		virtual void getTimingInfos(double &data_period, double &arrival_delay)
			{ data_period = this->data_period; arrival_delay = this->arrival_delay; }
		virtual void setTimingInfos(double data_period, double arrival_delay)
			{ this->data_period = data_period; this->arrival_delay = arrival_delay; }
		
		
		virtual double getLastTimestamp() = 0;
		
		VecIndT getRaws(double t1, double t2); /// will also release the raws before the first one
		int getUnreadRawInfos(RawInfos &infos); /// get timing informations about unread raws
		int getNextRawInfo(RawInfo &info); /// get info about next unread raw
		void getRaw(unsigned id, T& raw); /// will also release the raws before this one
		double getRawTimestamp(unsigned id);
		int getLastUnreadRaw(T& raw); /// will also release the raws before this one
		void getLastProcessedRaw(T& raw) { raw = buffer(last_sent_pos); } /// for information only (display...)
		void release() { release(read_pos); }
		
		friend class rtslam::SensorProprioAbstract;
		friend class rtslam::SensorExteroAbstract;
};


class HardwareSensorProprioAbstract: public HardwareSensorAbstract<RawVec>
{
	public:
	/*
		TODO return some automatic information about what is provided
		
	enum locpart {
		x=1,y=2,z=4,pos=7, 
		qw=8,qx=16,qy=32,qz=64,q=120,
		vx=128,vy=256,vz=512,vel=903,
		vyaw=1024,vpitch=2048,vroll=4096,vela=7288
	};
	locpart bor(locpart a, locpart b) { return (locpart)((unsigned)a | (unsigned)b); }
	locpart bor(locpart a, locpart b, locpart c) { return (locpart)((unsigned)bor(a,b) | (unsigned)c); }
	locpart bor(locpart a, locpart b, locpart c, locpart d) { return (locpart)((unsigned)bor(a,b,c) | (unsigned)d); }
	locpart bor(locpart a, locpart b, locpart c, locpart d, locpart e) { return (locpart)((unsigned)bor(a,b,c,d) | (unsigned)e); }
	locpart bor(locpart a, locpart b, locpart c, locpart d, locpart e, locpart f) { return (locpart)((unsigned)bor(a,b,c,d,e) | (unsigned)f); }
	
	virtual ind_array get(locpart x);
	*/
	
		HardwareSensorProprioAbstract(kernel::VariableCondition<int> &condition, unsigned bufferSize):
			HardwareSensorAbstract<RawVec>(condition, bufferSize) {}
		virtual int dataSize() = 0; /// number of measure variables provided (without timestamp and variance)
		virtual int varianceSize() = 0; /// number of variance variables provided
	
	
};

class HardwareSensorExteroAbstract: public HardwareSensorAbstract<raw_ptr_t>
{
	public:
		HardwareSensorExteroAbstract(kernel::VariableCondition<int> &condition, unsigned bufferSize):
			HardwareSensorAbstract<raw_ptr_t>(condition, bufferSize) {}
	
	
};

typedef boost::shared_ptr<hardware::HardwareSensorExteroAbstract> hardware_sensorext_ptr_t;
typedef boost::shared_ptr<hardware::HardwareSensorProprioAbstract> hardware_sensorprop_ptr_t;

#endif

#if 0
class HardwareSensorAbstract
{
	protected:
		boost::condition_variable &rawdata_condition;
		boost::mutex &rawdata_mutex;
	public:
		HardwareSensorAbstract(boost::condition_variable &rawdata_condition, boost::mutex &rawdata_mutex):
			rawdata_condition(rawdata_condition), rawdata_mutex(rawdata_mutex) {}
		/**
		@param rawPtr the latest raw available from the sensor
		@return the number of missed raws, -1 if no raw is available since last call, -2 if no raw will ever be available
		@note must be non blocking
		*/
		virtual int acquireRaw(raw_ptr_t &rawPtr) = 0;
		virtual ~HardwareSensorAbstract() {}
};
#endif


#if 1

template<typename T>
typename HardwareSensorAbstract<T>::VecIndT HardwareSensorAbstract<T>::getRaws(double t1, double t2)
{
	JFR_ASSERT(t1 <= t2, "");
	boost::unique_lock<boost::mutex> l(mutex_data);
	int i1, i2;
	int i, j;

	// find indexes by dichotomy
	int i_left = write_pos, i_right = write_pos + bufferSize-1;
	while(i_left != i_right)
	{
		j = (i_left+i_right)/2;
		i = j % bufferSize;
		if (extractRawTimestamp(buffer(i)) >= t1) i_right = j; else i_left = j+1;
	}
	i = i_left % bufferSize;
	i1 = (i-1 + bufferSize) % bufferSize;
	if (t1 <= 1.0 && extractRawTimestamp(buffer(i1)) < 0.0) i1 = i;
	bool no_larger = (extractRawTimestamp(buffer(i)) < t1);
	bool no_smaller = (i == write_pos);
	if (no_larger && extractRawTimestamp(buffer(i1)) < 0.0)  // no data at all
		return ublas::project(buffer, jmath::ublasExtra::ia_set(ublas::range(0,0)));
	if (no_smaller) JFR_ERROR(RtslamException, RtslamException::BUFFER_OVERFLOW, "Missing data: increase buffer size !");
	
	if (no_larger)
		i2 = i1;
	else
	{
		i_right = write_pos + bufferSize-1;
		while(i_left != i_right)
		{
			j = (i_left+i_right)/2;
			i = j % bufferSize;
			if (extractRawTimestamp(buffer(i)) >= t2) i_right = j; else i_left = j+1;
		}
		i = i_left % bufferSize;
		i2 = i;
	}
	
	
	// return mat_indirect
	read_pos = i1;
	l.unlock();
	cond_offline_freed.notify_all();

	if (i1 < i2)
	{
		return ublas::project(buffer, 
			jmath::ublasExtra::ia_set(ublas::range(i1,i2+1)));
	} else
	{
		return ublas::project(buffer, 
			jmath::ublasExtra::ia_concat(jmath::ublasExtra::ia_set(ublas::range(i1,buffer.size1())),
			                             jmath::ublasExtra::ia_set(ublas::range(0,i2+1))));
	}
}


template<typename T>
int HardwareSensorAbstract<T>::getUnreadRawInfos(RawInfos &infos)
{
	infos.available.clear();
	if (!isEmpty())
	{
		int first_stop, second_stop;
		int first = getFirstUnreadPos(), last = getLastUnreadPos();
		JFR_DEBUG("getUnreadRawInfos: first " << first << " last " << last);

		if (first <= last)
		{
			first_stop = last;
			second_stop = -1;
		} else
		{
			first_stop = bufferSize-1;
			second_stop = last;
		}
		
		for(int pos = first; pos <= first_stop; ++pos)
			infos.available.push_back(RawInfo(pos,extractRawTimestamp(buffer(pos)),extractRawArrival(buffer(pos))));
		for(int pos = 0; pos <= second_stop; ++pos)
			infos.available.push_back(RawInfo(pos,extractRawTimestamp(buffer(pos)),extractRawArrival(buffer(pos))));
	}
	
	double data_period, arrival_delay;
	getTimingInfos(data_period, arrival_delay);
	double next_date = getLastTimestamp() + data_period;
	infos.next = RawInfo(0,next_date,next_date+arrival_delay);
	infos.process_time = 0.;
	
	if (infos.available.size() == 0)
	{
		if (no_more_data) return -2; else return -1;
	}
	return 0;
}

template<typename T>
int HardwareSensorAbstract<T>::getNextRawInfo(RawInfo &info)
{
	if (!isEmpty())
	{
		int first = getFirstUnreadPos();
		info = RawInfo(first,extractRawTimestamp(buffer(first)),0.0);
		return 0;
	} else
	{
		if (no_more_data) return -2; else return -1;
	}
}

template<typename T>
double HardwareSensorAbstract<T>::getRawTimestamp(unsigned id)
{
	return extractRawTimestamp(buffer[id]);
}

template<typename T>
void HardwareSensorAbstract<T>::getRaw(unsigned id, T& raw)
{
	releaseUntil(id);
	raw = buffer[id];
	last_sent_pos = id;
	index.applyAndNotify(boost::lambda::_1++);
}

template<typename T>
int HardwareSensorAbstract<T>::getLastUnreadRaw(T& raw)
{
	boost::unique_lock<boost::mutex> l(mutex_data);
	int missed_count = data_count-1;
	if (data_count > 0)
	{
		unsigned id = getLastUnreadPos(true);
		releaseUntil(id, true);
		raw = buffer[id];
		last_sent_pos = id;
		boost::unique_lock<boost::mutex> l(mutex_data);
		data_count = 0;
		l.unlock();
		index.applyAndNotify(boost::lambda::_1++);
	}
	if (no_more_data && missed_count == -1) return -2; else return missed_count;
}


#endif


}}}

#endif

