#ifndef SPSC_INTERFACE
#define SPSC_INTERFACE

namespace SugRingBuffer { 

    template<typename T>
    class SPSCInterface {
    public:
        virtual ~SPSCInterface() = default;
        // Non-copyable, non-movable.
        SPSCInterface(const SPSCInterface&) = delete;
        SPSCInterface& operator=(const SPSCInterface&) = delete;
        SPSCInterface(SPSCInterface&&) = delete;
        SPSCInterface& operator=(SPSCInterface&&) = delete;

        virtual bool push(const T& item) = 0;
        virtual bool pop(T& item) = 0;
        [[nodiscard]] virtual bool full() const = 0;
        [[nodiscard]] virtual bool empty() const = 0;
        virtual size_t size() const = 0;
    
    protected:
        SPSCInterface() = default;

    };

};  //namespace SugRingBuffer

#endif