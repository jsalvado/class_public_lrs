/******************************************************************************************************/
/*                           HYREC: Hydrogen and Helium Recombination Code                            */
/*                      Written by Yacine Ali-Haimoud and Chris Hirata (Caltech)                      */
/*                                                                                                    */
/*     energy_injection.c: functions for the energy injection rate by various physical processes      */
/*                                                                                                    */
/*     Written October 2016                                                                          */
/*                                                                                                    */
/******************************************************************************************************/

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "include/hyrectools.h"
#include "include/energy_injection.h"


/***************************************************************************************
Total volumic rate of energy *injection*, in eV/cm^3/s due to DM annihilation
in the smooth background and in haloes as in Giesen et al 1209.0247
****************************************************************************************/

double dEdtdV_DM_ann(double z, INJ_PARAMS *params){

  double pann_tot, u_min, erfc;
  double zp1, zp1_ann, zp1_max, zp1_min, var, zp1_halo;

  var       = params->ann_var;
  zp1       = z + 1.;
  zp1_ann   = params->ann_z + 1.;
  zp1_max   = params->ann_zmax + 1.;
  zp1_halo  = params->ann_z_halo + 1.;
  zp1_min   = params->ann_zmin + 1.;

  pann_tot = 0.;

  /* Dark matter annihilation in the smooth background */
  if (params->pann > 0.) {

    /* Parametrized variation of pann */
    if (zp1 > zp1_max) pann_tot = params->pann *exp(-var *square(log(zp1_ann/zp1_max)));
    else if (zp1 > zp1_min) {
      pann_tot = params->pann *exp(var*(-square(log(zp1_ann/zp1_max))
				+square(log(zp1/zp1_max))));
    }
    else {
      pann_tot = params->pann *exp(var*(-square(log(zp1_ann/zp1_max))
				+square(log(zp1_min/zp1_max))));
    }
    pann_tot*=zp1*zp1*zp1;
  }
  /* Dark matter annihilation in haloes */
  if (params->pann_halo > 0.) {
    u_min = zp1/zp1_halo;
    erfc  = pow(1.+0.278393*u_min+0.230389*u_min*u_min+0.000972*u_min*u_min*u_min+0.078108*u_min*u_min*u_min*u_min,-4);
    pann_tot += params->pann_halo *erfc;
  }

  return square(10537.4*params->odmh2) * zp1*zp1*zp1 *1e-9* pann_tot;
  /* the prefactor is 3 H100^2/(8 Pi G) c^2 in eV/cm^3, H100 = 100km/s/Mpc */
  /* pann is given in cm^3/s/GeV, multiply by 1e-9 to get cm^3/s/eV */

}

/***************************************************************************************
Effect of accreting primordial black holes
Since the accuracy is certainly not at the percent level,
we assume best-fit values for all cosmological parameters and neglect Helium
Throughout, Mpbh is in solar masses, Teff in Kelvins
***************************************************************************************/


/* Dimensionless Compton drag rate */
double beta_pbh(double Mpbh, double z, double xe, double Teff) {
  double a, vB, tB;

  a     = 1./(1.+z);
  vB    = 9.09e3 * sqrt((1.+xe)*Teff);    /* Bondi speed in cm/s */
  tB    = 1.33e26 *Mpbh/vB/vB/vB;         /* Bondi timescale in sec*/

  return 7.45e-24 *xe/a/a/a/a *tB;
}

 /* Dimensionless Compton cooling rate */
double gamma_pbh(double Mpbh, double z, double xe, double Teff) {
  return  3.67e3/(1.+xe) *beta_pbh(Mpbh, z, xe, Teff);
}

/* Dimensionless accretion rate */
double lambda_pbh(double Mpbh, double z, double xe, double Teff) {
  double beta, gamma, lam_ricotti, lam_ad, lam_iso, lam_nodrag;

  beta  = beta_pbh(Mpbh, z, xe, Teff);
  gamma = gamma_pbh(Mpbh, z, xe, Teff);

  lam_ricotti = exp(4.5/(3.+pow(beta, 0.75)))/square(sqrt(1.+beta)+1.);
  /* Fitting formula from Ricotti 2007 for the fully isothermal case */
  lam_ad      = pow(0.6, 1.5)/4.;
  lam_iso     = exp(1.5)/4.;

  lam_nodrag = lam_ad + (lam_iso - lam_ad) * pow(gamma*gamma/(88. + gamma*gamma), 0.22);
  /* Fitting formula for the no-drag case */

  return lam_ricotti *lam_nodrag /lam_iso;
}

/* Accretion rate (in g/s), accounting for Compton drag and Compton cooling.*/
/* This is assuming Omega_b h^2 = 0.022 (sufficient at the level of accuracy we need) */
double Mdot_pbh(double Mpbh, double z, double xe, double Teff) {

  double vB  = 9.09e3 * sqrt((1.+xe)*Teff);    /* Bondi speed in cm/s */

  return 9.15e22 * Mpbh*Mpbh*cube((1.+z)/vB) *lambda_pbh(Mpbh, z, xe, Teff);
}

/* Temperature of the flow near the Shchwarzschild radius divided by m_e c^2 */
/** Changed 01/26/2017: added a parameter "coll_ion":
    if set to 1, assume collisional ionizations
    if set to 0, assume photoionizations.
**/

double TS_over_me_pbh(double Mpbh, double z, double xe, double Teff, int coll_ion) {
  double gamma, tau, YS;

  gamma = gamma_pbh(Mpbh, z, xe, Teff);

  tau = 1.5/(5. + pow(gamma, 2./3.));     /* T/Teff -> tau *rB/r        for r << rB */

  YS = 2./(1.+xe) * tau/4. *pow(1.-2.5*tau,1./3.)*1836.;

  if (coll_ion == 1) YS *= pow((1.+ xe)/2., 8.);

  return YS /pow(1.+YS/0.27, 1./3.);
}

/* Radiative efficiency divided by \dot{m} */
double eps_over_mdot_pbh(double Mpbh, double z, double xe, double Teff, int coll_ion) {
  double X, G;

  X = TS_over_me_pbh(Mpbh, z, xe, Teff, coll_ion);

  /* Fit to the (e-e + e-p) free-free Gaunt factor */
  if (X < 1) G = 4./M_PI * sqrt(2./M_PI/X) *(1.+ 5.5*pow(X, 1.25));
  else       G = 13.5/M_PI *(log(2.*X*0.56146 + 0.08) + 4./3.);

  return X/1836./137. * G;   /* alpha[fine-struct] * TS/me *mp/me * G */
}

/* Luminosity of a single PBH in erg/s*/
double L_pbh(double Mpbh, double z, double xe, double Teff, int coll_ion) {
  double Mdot, mdot, eff;

  Mdot = Mdot_pbh(Mpbh, z, xe, Teff);
  mdot = Mdot / (1.4e17 * Mpbh);        /* Mdot c^2 / L_Eddington */
  eff  = mdot *eps_over_mdot_pbh(Mpbh, z, xe, Teff, coll_ion);

  return eff * Mdot * 9e20;  /* L = epsilon Mdot c^2 */
}

/* Very approximate value of the rms relative velocity, in cm/s */
double vbc_rms_func(double z) {
  if (z < 1e3) return 3e6 * (1.+z)/1e3;
  else         return 3e6;
}

/* Average of the pbh luminosity (erg/s) over the distribution of relative velocities */

double L_pbh_av(double Mpbh, double z, double xe, double Tgas, int coll_ion) {
  double vbc_rms, vbc_max, vbc, P_vbc, num, denom, x, Teff;
  int i, Nvbc;

  Nvbc = 50; // More than enough at the level of precision we use

  vbc_rms = vbc_rms_func(z);

  vbc_max = 5.*vbc_rms;

  num = denom = 0.;
  for (i = 0; i < Nvbc; i++) {
    vbc    = i*vbc_max/(Nvbc-1.);
    x      = vbc/vbc_rms;
    denom += P_vbc = x*x* exp(-1.5*x*x);

    Teff = Tgas + 1.21e-8 *vbc*vbc/(1.+xe);

    num += L_pbh(Mpbh, z, xe, Teff, coll_ion) * P_vbc;
  }

  return num/denom;
}

/* Rate of energy *injection* per unit volume (in eV/cm^3/s) due to PBHs */
/* Taking Omega_c h^2 = 0.12                                             */

double dEdtdV_pbh(double fpbh, double Mpbh, double z, double xe, double Tgas, int coll_ion) {
  double xe_used = (xe < 1.? xe : 1.); /* Since we are not accounting for Helium */

  // xe_used = feedback + (1-feedback)*xe_used;
  // Old feedback - no feedback model, where xe = 1 throughout was assumed in the strong feedback case

  if (fpbh > 0.) {
    return 7.07e-52/Mpbh * cube(1.+z) * fpbh *L_pbh_av(Mpbh, z, xe_used, Tgas, coll_ion);
  }
  else return 0.;
}

/***********************************************************************************
Total energy *injection* rate per unit volume
Add in your favorite energy injection mechanism here
***********************************************************************************/

double dEdtdV_inj(double z, double xe, double Tgas, INJ_PARAMS *params){
  return   dEdtdV_DM_ann(z, params)
    + dEdtdV_pbh(params->fpbh, params->Mpbh, z, xe, Tgas, params->coll_ion);
}


/**********************************************************************************
Energy *deposition* rate per unit volume
Essentially use a very simple ODE solution
**********************************************************************************/

void update_dEdtdV_dep(double z_out, double dlna, double xe, double Tgas,
		       double nH, double H, INJ_PARAMS *params, double *dEdtdV_dep) {

  double inj  = dEdtdV_inj(z_out, xe, Tgas, params);

  if (params->on_the_spot == 1){
    *dEdtdV_dep = inj;
  }

  // Else put in your favorite recipe to translate from injection to deposition
  // Here I assume injected photon spectrum Compton cools at rate dE/dt = - 0.1 n_h c sigma_T E
  // This is valid for E_photon ~ MeV or so.

  else { // 0.1 c sigma_T = 2e-15 (cgs).
    *dEdtdV_dep = (*dEdtdV_dep *exp(-7.*dlna) + 2e-15* dlna*nH/H *inj)
                 /(1.+ 2e-15 *dlna*nH/H);
  }
  //  printf("on_the_spot %d *dEdtdV_dep %e inj %e \n", params->on_the_spot,*dEdtdV_dep,inj);


}

/*******************************************************************************
Fraction of energy deposited in the form of heat, ionization and excitations
*******************************************************************************/

double chi_heat(double xe) {
  return (1.+2.*xe)/3.; // Approximate value of Chen & Kamionkowski 2004

  // fit by Vivian Poulin of columns 1 and 2 in Table V of Galli et al. 2013
  // overall coefficient slightly changed by YAH so it reaches exactly 1 at xe = 1.
  // return (xe < 1.? 1.-pow(1.-pow(xe,0.300134),1.51035) : 1.);
}

double chi_ion(double xe) {
  return (1.-xe)/3.; // Approximate value of Chen & Kamionkowski 2004

  // fit by Vivian Poulin of columns 1 and 4 in Table V of Galli et al. 2013
  // return 0.369202*pow(1.-pow(xe,0.463929),1.70237);
}

double chi_exc(double xe) {
  return 1. - chi_ion(xe) - chi_heat(xe);
}
