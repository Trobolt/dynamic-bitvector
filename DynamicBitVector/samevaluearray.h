#ifndef ADS_SAMEVALUEARRAY_H
#define ADS_SAMEVALUEARRAY_H

#include "../myassert.h"


template<class SizeType, class ArrType>
struct SameValueArray {
    static_assert(std::is_integral<SizeType>::value);
    static_assert(std::is_integral<ArrType>::value);
private:
    SizeType _size = 0;
    ArrType arr[3] = {0, 0, 0};
    ASSERT_CODE_GLOBALE(
            ArrType
                    assert_var = arr[0];
            int assert_counter = 1;
    )


public:

    explicit SameValueArray(SizeType size) : _size(size - (2 + 1)) {}

    explicit SameValueArray() : _size(0 - (2 + 1)) {}

    inline void clear(SizeType newSize) {
        _size = newSize - (2 + 1);
        arr[0] = 0;
        arr[1] = 0;
        arr[2] = 0;
        ASSERT_CODE_LOCALE(assert_var = arr[0], assert_counter = 1;)
        ASSERT_E((&paccess(size() - 1)), &arr[2])
        ASSERT_PE(size() > 1, &paccess(size() - 2), &arr[1])
    }

    inline void incSize() {
        ASSERT_E(arr[0], arr[1])
        _size += 1;
        arr[1] = arr[2];
        arr[2] = 0;
    }

    inline SizeType size() {
        return _size + (2 + 1);
    }

    inline uint64_t sum() {
        uint64_t out = 0;
        switch(_size) {
            default:
                out = (_size + 1) * arr[0];
                [[fallthrough]];
            case sc<SizeType>(-1):
                out += arr[1];
                [[fallthrough]];
            case sc<SizeType>(-2):
                return out + arr[2];
        }
    }

    inline bool sumEqualsOne() {
        ASSERT_E(_size == sc<SizeType>(-2) & arr[2] == 1, sum() == 1)
        return _size == sc<SizeType>(-2) & arr[2] == 1;
    }

    inline void setAll(const ArrType val) {
        ASSERT_NE(val, 0)
        ASSERT_B(assert_counter > 0 || assert_var == arr[0], "default var has been set twice")
        arr[0] = val;
        arr[1] = val;
        arr[2] = val;
        ASSERT_CODE_LOCALE(
                if(assert_var != arr[0])--assert_counter, assert_var = arr[0];)
    }

    inline void setLast(const ArrType val) {
        arr[2] = val;
        ASSERT_PE(size() > 0, &operator[](size() - 1), &arr[2])
    }

    inline void setNextToLast(const ArrType val) {
        arr[1] = val;
        ASSERT_PE(size() > 1, &operator[](size() - 2), &arr[1])
    }

    inline void setFirstN(const ArrType val) {
        ASSERT_CODE_LOCALE(assert_counter = 0;
                                   assert_var = val;)
        arr[0] = val;
        ASSERT_PE(size() > 2, &operator[](size() - 3), &arr[0])
    }

private:
    const inline ArrType &paccess(const SizeType idx) const {
        if constexpr(std::is_signed<SizeType>::value) {
            if(idx < _size) {
                return arr[0];
            } else {
                return arr[idx - _size];
            }
        } else {
            //TODO: error
            if(idx > _size) { return arr[idx - _size]; }
            else { return arr[0]; }
        }
    }

public:
    const inline ArrType &operator[](const SizeType idx) NO_ASSERT_CODE(const) {
        ASSERT_E(assert_counter, 0)
        ASSERT_B(assert_counter > 0 || assert_var == arr[0], "default var has been set twice")
        ASSERT_CODE_LOCALE(
                if(assert_var != arr[0])--assert_counter, assert_var = arr[0];)
        ASSERT_R(idx, 0, size())
        return paccess(idx);
    }

    template<ArrType border>
    inline void distributeEvenly(const ArrType val) {
        // ASSERT_E(arr[2], 0)
        if(_size != -3) {
            if(val < border) {
                arr[2] += val;
                //decSize();
            } else {
                incSize();
                const ArrType t = arr[1] + val;
                arr[1] = DIV_ROUNDUP(t, 2);
                arr[2] = t / 2;
            }
        } else {
            incSize();
            arr[2] = val;
        }
        ASSERT_PE(size() != 0, operator[](size() - 1), arr[2])
        ASSERT_E(&operator[](size() - 1), &arr[2])
        ASSERT_PE(size() > 1, &operator[](size() - 2), &arr[1])
    }

};

#endif //ADS_SAMEVALUEARRAY_H
