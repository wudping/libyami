/*
 * Copyright (C) 2014-2016 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef fraction_h
#define fraction_h
#include <iostream>
#include <algorithm>
#include <stdint.h>

using namespace std;

class Fraction{
public:
    Fraction():m_num(0), m_den(1){}

    Fraction(int32_t num, int32_t den);

    void operator += (const Fraction& data);

    Fraction operator + (const Fraction& data) const;
    
    void operator -= (const Fraction& data);
    
    Fraction operator - (const Fraction& data) const;

    void operator *= (Fraction data);

    Fraction operator * (Fraction data) const;

    void operator /= (Fraction data);

    Fraction operator / (const Fraction& data) const;

    float floatValue()  const;
    int32_t intValue()  const;

    void print() const{cout << m_num << "/" << m_den << endl;}
    void setNumerator(int32_t num){  m_num = num;}
    void setDenominator(int32_t den){ m_den = den; }
    int32_t getNumerator() const{ return m_num;}
    int32_t getDenominator() const{ return m_den;}
    bool isZero() const{return (! m_num) && (m_den);}
    bool isInteger() const{return !(m_num % m_den);}
private:
    int32_t m_num;
    int32_t m_den;
};


#endif /* fraction_h */


